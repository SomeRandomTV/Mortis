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
        std::cerr << "ERROR: V-Node does not exist in connected_nodes\n";
    } else {
        std::cout << "Killing connection to node: " << _id;
        close(connected_nodes[_id].node_sock);
    }

}

/**
 * the whole accept-a-node dance, start to finish: opens the listening
 * socket, binds, listens, blocks on accept() for exactly one connection,
 * then blocks again waiting for that connection's handshake. bails out
 * (closing whatever's open so far) the moment anything goes wrong -
 * socket creation, bind, listen, accept, no handshake received, wrong
 * message type instead of a handshake, or an unsupported protocol
 * version.
 *
 * on success, registers the node in connected_nodes and remembers its
 * vnode_id in _active_vnode_id so recv_loop() knows which socket to
 * read from next.
 *
 * still single-connection - only ever accepts once, doesn't loop back to
 * accept() for a second node (SCRUM-12/SCRUM-19). also doesn't do
 * anything to actually threaten-enforce "start timer for handshake" /
 * "stop timer" below, those are just comments marking where a real
 * handshake timeout would need to go in
 *
 * @return 0 on a fully accepted + handshaken connection, otherwise a
 *         distinct negative error code per failure point (-1 socket, -2
 *         inet_pton, -3 bind, -4 listen, -5 accept, -6 wrong message
 *         type, -7 bad protocol version, -8 recv_event came back nullopt)
 */
int MortisHub::accept_connection() {

    std::cout << "HUB serving at " << _HOST << ":" << _PORT << std::endl;
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        perror("Socket creation failed");
        close(sock);
        return -1;
    }

    sockaddr_in hub_addr;
    hub_addr.sin_family = AF_INET;
    hub_addr.sin_port = htons(_PORT);

    int _c = inet_pton(AF_INET, _HOST, &hub_addr.sin_addr.s_addr);

    if (_c < 1) {
        std::cerr  << "ERROR: Could not convert host to binary" << std::endl;
        close(sock);
        return -2;
    }

    int _b = bind(sock, reinterpret_cast<sockaddr *>(&hub_addr), sizeof(hub_addr));

    if (_b != 0) {
        perror("bind");
        close(sock);
        return -3;
    }

    std::cout << "Listening ... \n" << std::endl;
    int _l = listen(sock, 5);
    if (_l == -1) {
        perror("listen");
        close(sock);
        return -4;
    }

    // ---- TODO: Multi-thread ----
    sockaddr_in n_addr;
    socklen_t n_addr_len = sizeof(n_addr);
    int _connection = accept(sock, reinterpret_cast<sockaddr *>(&n_addr), &n_addr_len);

    if (_connection == -1) {
        perror("accept");
        return -5;
    };


    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &n_addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << "Node connecting from " << ip_str << ":" << ntohs(n_addr.sin_port) << std::endl;
    std::cout << "Waiting for handshake ...\n";
    // -----------------------------

    // start timer for handshake
    std::optional<Envelope> handshake {recv_event(_connection)};

    if (handshake == std::nullopt) {
        std::cerr << "ERROR: Failed to receive handshake ... DISCONNECTING \n";
        close(_connection);
        return -8;
    }

    if (handshake->type != MessageType::Handshake) {
        std::cerr << "ERROR: Expected handshake but got " << msg_to_string(handshake->type) << " ... DISCONNECTING \n";
        close(_connection);
        return -6;
    }
    // stop timer
    std::cout << "Received Handshake ... parsing \n";

    uint8_t _vnode_id = handshake->vnode_id;
    std::string_view proto_v = handshake->version;

    // pretty sure there is a better way of doing this
    // i need to find a minium version required LATER
    // right now anything other than v0.0.0 is wrong
    if (proto_v != "0.0.0") {
        std::cerr << "ERROR: Protocol Version not supported ... DISCONNECTING \n";
        close(_connection);
        return -7;
    }

    std::cout << "Connection to V-Node " << std::format("{}", _vnode_id) << " @ " << ip_str << " accepted\n";
    ConnectedNode valid_node {
        ip_str,                          // node_addr
        true,                            // is_connected
        15,                              // heartbeat timer
        connection_state::CONNECTION_STATES::ACTIVE,       // node_state: AWAITING_HANDSHAKE -> ACTIVE
        _connection     // node socket
    };

    connected_nodes[_vnode_id] = valid_node;
    _active_vnode_id = _vnode_id;

    std::cout << "Connected ... " << std::endl;
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
                std::cout << "Got heartbeat\n";
                // reset timer
                break;
            case MessageType::InferenceEvent:
                std::cout << "Got Inference Event\n";
                // reset timer
                std::cout << "Time: " << env->timestamp_ms << "\n";
                std::cout << "From ID: " << env->vnode_id << "\n";
                std::cout << "========== Payload ==========\n";
                std::cout << env->payload.dump(2);
                std::cout << "=============================\n";
                break;
            case MessageType::SHUTDOWN:
                std::cout << "Shutdown signal received ... shutting down connection ... \n";
                kill_node_connection(env->vnode_id);
                return;
            default:

                std::cerr << "ERROR: Invalid Envelope ... DISCONNECTING \n";
                std::cerr << "Got type: " << msg_to_string(env->type) << "\n";
                // retry here?
                kill_node_connection(env->vnode_id);
                return;
        }

    }

    std::cout << "it is done ... ";

}

