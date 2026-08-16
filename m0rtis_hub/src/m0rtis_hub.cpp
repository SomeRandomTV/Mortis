#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <optional>

#include "m0rtis_hub.hpp"
#include "m0rtis_proto/envelope.hpp"
#include "m0rtis_proto/framing.hpp"
#include "m0rtis_proto/logging.hpp"
#include "m0rtis_proto/message_type.hpp"

using namespace m0rtis;

/**
 * closes a node's socket and tells you if it even found the node first.
 * doesn't touch connected_nodes otherwise - the map entry sticks around
 * with is_connected still true and node_state still whatever it was, so
 * don't go looking at connected_nodes[_id] after this and expect it to
 * look "disconnected"
 *
 * @param _id vnode_id of the connection to kill - looked up in connected_nodes
 */
void MortisHub::kill_node_connection(uint8_t _id) {

    if (connected_nodes.find(_id) == connected_nodes.end()) {
        log::error("V-Node does not exist in connected_nodes", {log::field("vnode_id", _id)});
    } else {
        log::info("Killing connection to node", {log::field("vnode_id", _id)});
        close(connected_nodes[_id].node_sock);
    }

}


void MortisHub::heartbeat_daemon() {
    pid_t hd_pid { fork() };

    if (hd_pid < 0) {
        log::error("Failed to create heartbeat daemon", {log::field("errno", strerror(errno))});
        exit(EXIT_FAILURE);
    }

    if (hd_pid > 0) {
        exit(EXIT_SUCCESS);
    }

}


int MortisHub::accept_connection() {

    log::info("HUB serving", {log::field("host", _HOST), log::field("port", _PORT)});
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        log::error("Socket creation failed", {log::field("errno", strerror(errno))});
        close(sock);
        return -1;
    }

    sockaddr_in hub_addr;
    hub_addr.sin_family = AF_INET;
    hub_addr.sin_port = htons(_PORT);

    int _c = inet_pton(AF_INET, _HOST, &hub_addr.sin_addr.s_addr);

    if (_c < 1) {
        log::error("Could not convert host to binary");
        close(sock);
        return -2;
    }

    int _b = bind(sock, reinterpret_cast<sockaddr *>(&hub_addr), sizeof(hub_addr));

    if (_b != 0) {
        log::error("bind failed", {log::field("errno", strerror(errno))});
        close(sock);
        return -3;
    }

    log::info("Listening ...");
    int _l = listen(sock, 5);
    if (_l == -1) {
        log::error("listen failed", {log::field("errno", strerror(errno))});
        close(sock);
        return -4;
    }

    // ---- TODO: Multi-thread ----
    sockaddr_in n_addr;
    socklen_t n_addr_len = sizeof(n_addr);
    int _connection = accept(sock, reinterpret_cast<sockaddr *>(&n_addr), &n_addr_len);

    if (_connection == -1) {
        log::error("accept failed", {log::field("errno", strerror(errno))});
        return -5;
    };


    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &n_addr.sin_addr, ip_str, sizeof(ip_str));
    log::info("Node connecting", {log::field("addr", ip_str), log::field("port", ntohs(n_addr.sin_port))});
    log::info("Waiting for handshake ...");
    // -----------------------------

    // start timer for handshake
    std::optional<Envelope> handshake {recv_event(_connection)};

    if (handshake == std::nullopt) {
        log::error("Failed to receive handshake ... DISCONNECTING");
        close(_connection);
        return -8;
    }

    if (handshake->type != MessageType::Handshake) {
        log::error("Expected handshake but got a different type ... DISCONNECTING", {log::field("type", msg_to_string(handshake->type))});
        close(_connection);
        return -6;
    }
    // stop timer
    log::info("Received Handshake ... parsing");

    uint8_t _vnode_id = handshake->vnode_id;
    std::string_view proto_v = handshake->version;

    // pretty sure there is a better way of doing this
    // i need to find a minium version required LATER
    // right now anything other than v0.0.0 is wrong
    if (proto_v != "0.0.0") {
        log::error("Protocol Version not supported ... DISCONNECTING", {log::field("version", proto_v)});
        close(_connection);
        return -7;
    }

    log::info("Connection accepted", {log::field("vnode_id", _vnode_id), log::field("addr", ip_str)});
    ConnectedNode valid_node {
        ip_str,                          // node_addr
        true,                            // is_connected
        15,                              // heartbeat timer
        connection_state::CONNECTION_STATES::ACTIVE,       // node_state: AWAITING_HANDSHAKE -> ACTIVE
        _connection     // node socket
    };

    connected_nodes[_vnode_id] = valid_node;
    _active_vnode_id = _vnode_id;

    log::info("Connected ...");
    return 0;


}

/**
 * services the one node accept_connection() just registered - reads
 * envelopes off connected_nodes[_active_vnode_id].node_sock in a loop
 * (capped at 10 iterations right now, that's scaffolding/smoke-test
 * territory, not a real "run forever" dispatch loop yet) and reacts
 * based on type: Heartbeat just logs, InferenceEvent logs + dumps the
 * payload, SHUTDOWN and anything else (default - i.e. a protocol
 * violation) both kill the connection and return out of the loop
 * entirely.
 *
 * bails (break) the moment recv_event comes back nullopt, rather than
 * retrying - a nullopt here always means the connection's dead one way
 * or another, there's no case where trying again makes sense with how
 * recv_event works today (see the comment right above the nullopt check)
 */
void MortisHub::recv_loop() {

    int counter{0};
    while (counter < 10) {

        std::optional<Envelope> env = recv_event(connected_nodes[_active_vnode_id].node_sock);

        // ok but a nullopt can mean different things
        // if i get a nullopt because peer closed connection - ok close connection
        // if something fails do i actually quit or do i do i send a retry request -> gonna hold on to this
        // need to design a rety mechanism
        // fail for now
        if (env == std::nullopt) {
            break;
        }

        switch (env->type) {
            case MessageType::Heartbeat:
                log::info("Got heartbeat", {log::field("vnode_id", env->vnode_id)});
                // reset timer
                break;
            case MessageType::InferenceEvent:
                log::info("Got Inference Event", {
                    log::field("vnode_id", env->vnode_id),
                    log::field("time", env->timestamp_ms),
                    log::field("payload", env->payload.dump(2)),
                });
                break;
            case MessageType::SHUTDOWN:
                log::info("Shutdown signal received ... shutting down connection ...", {log::field("vnode_id", env->vnode_id)});
                kill_node_connection(env->vnode_id);
                return;
            default:

                log::error("Invalid Envelope ... DISCONNECTING", {
                    log::field("vnode_id", env->vnode_id),
                    log::field("type", msg_to_string(env->type)),
                });
                // retry here?
                kill_node_connection(env->vnode_id);
                return;
        }

    }

    log::info("it is done ...");

}

