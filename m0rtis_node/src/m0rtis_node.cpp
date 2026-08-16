#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctime>
#include <thread>
#include <chrono>

#include "m0rtis_node.hpp"
#include "m0rtis_proto/envelope.hpp"
#include "m0rtis_proto/framing.hpp"
#include "m0rtis_proto/event_type.hpp"
#include "m0rtis_proto/logging.hpp"
#include "m0rtis_proto/message_type.hpp"

using namespace m0rtis;


/* =========== Private Methods =========== */

void MortisNode::heartbeat_daemon() {
    /* Runs a timer in the background
     * After timer reaches 0 
     * Emit heartbeat
     * Reset Timer 
     */

    pid_t hd_pid = fork();  // heartbeat_daemon PID 
    
    if (hd_pid < 0) {
        log::error("fork failed", {log::field("errno", strerror(errno))});
        exit(EXIT_FAILURE);
    }
    // detaches
    if (hd_pid > 0) {
       exit(EXIT_SUCCESS);
    }
    // creates separate session
    if (setsid() < 0) {
        log::error("setsid failed", {log::field("errno", strerror(errno))});
        exit(EXIT_FAILURE);
    }

    // ok so on a timer - send a heartbeat to the server, now
    // a separate socket sounds like a good descision
    _heartbeat_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_heartbeat_sock < 0) {
        log::error("Failed to create heartbeat socket", {log::field("errno", strerror(errno))});
        exit(EXIT_FAILURE);
    }







    
}
/* ========== Public API Methods ==========*/

/**
 * opens the TCP socket, connects to the hub at _HOST:_PORT, and sends
 * the one-and-only handshake envelope every connection has to start
 * with. bails out (closing whatever's open so far) the moment any step
 * fails - socket creation, address conversion, connect(), or the hub
 * flat-out rejecting the handshake.
 *
 * doesn't touch event_loop() or anything past the handshake - this is
 * just "get connected and say hello," nothing else
 *
 * @return 0 if the hub accepted the handshake, otherwise a negative
 *         error code (-1 socket, -2 inet_pton, -3 connect, or
 *         whatever emit_event returned if the handshake send itself failed)
 */
int MortisNode::connect_hub() {

    log::info("Node connecting to Hub", {log::field("host", _HOST), log::field("port", _PORT)});

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        log::error("Failed to create socket", {log::field("errno", strerror(errno))});
        close(sock);
        return -1;
    }

    sockaddr_in node_addr{};
    socklen_t node_len = sizeof(node_addr);
    node_addr.sin_family = AF_INET;     // IPv4 192.126.12.123
    node_addr.sin_port = htons(_PORT);

    int _c = inet_pton(AF_INET, _HOST, &node_addr.sin_addr);
    if (_c < 1) {
        log::error("Failed to convert host IP from string to binary");
        close(sock);
        return -2;
    }

    int _connection = connect(sock, reinterpret_cast<sockaddr *>(&node_addr), node_len);
    if (_connection < 0) {
        log::error("connect failed", {log::field("errno", strerror(errno))});
        close(sock);
        return -3;
    }
    time_t right_now;
    Envelope env {
       "0.0.0",     // protocol version
        MessageType::Handshake,
        VNODE_ID,   // vision node id
        time(&right_now),   // right fucking now
        nlohmann::json::object()    // empty payload

    };

    log::info("Sending Handshake ...");

    int err = emit_event(sock, env);

    if (err != 0) {
        close(sock);
        close(_connection);
        log::error("Hub rejected the handshake", {log::field("err", err)});
        return err;

    }

    log::info("Hub accepted connection");
    return 0;


}

/**
 * the node's main loop, post-handshake - runs a fixed 10 ticks (not
 * actually indefinite despite the header comment, that's still TODO),
 * alternating between pulling a fake inference event off
 * next_fixture_inference_event() on even ticks and sending a heartbeat
 * on odd ticks, sleeping 900ms between each. calls node_shutdown() once
 * the loop's done so the hub gets a clean SHUTDOWN instead of just
 * seeing the socket vanish.
 *
 * bails early (closing the socket, no shutdown envelope) if any
 * emit_event call fails partway through - no retry, no partial-cleanup
 * dance, just stop.
 *
 * heartbeat is still sent inline on the same thread/tick as everything
 * else - the "needs to be a background process" comment below is
 * flagging that this isn't really independent of event traffic yet, a
 * slow/blocked event send currently delays the next heartbeat too
 */
void MortisNode::event_loop() {

    int counter { 0 };
    int err { 0 };
    while (counter < 10) {

        if (counter % 2 == 0) {
            Envelope inf = next_fixture_inference_event();

            log::info("Got Inference Event from the I.R.I.S.", {
                log::field("time", inf.timestamp_ms),
                log::field("version", inf.version),
            });

            err = emit_event(sock, inf);

            if (err != 0) {
                log::error("Could not send data", {log::field("err", err)});
                close(sock);
                return;
            }

            log::info("Envelope sent ...");

        }


        /* ========== This needs to be a background process ========== */
        if (counter % 2 == 1) {
            time_t right_now;

            Envelope heartbeat {
                "0.0.0",
                MessageType::Heartbeat,
                VNODE_ID,
                time(&right_now),
                nlohmann::json::object()

            };
            err = emit_event(sock, heartbeat);

            if (err != 0) {
                log::error("Could not send heartbeat", {log::field("err", err)});
                close(sock);
                return;
            }
        }
        counter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }
    node_shutdown();
}


/**
 * tells the hub this node is disconnecting on purpose - sends one
 * SHUTDOWN envelope, then closes the socket either way (whether the send
 * worked or not, there's no point holding the socket open after this).
 * this is what lets recv_loop() on the hub side tell "node said goodbye"
 * apart from "node just vanished."
 *
 * @return 0 if the shutdown envelope actually made it out, otherwise
 *         whatever negative error code emit_event returned - either way
 *         the socket's closed by the time this returns
 */
int MortisNode::node_shutdown() {
    // this function is to notify the hub that it is shutting down

    time_t right_fucking_now;
    Envelope shutdown {
        PROTOCOL_VERSION,
        MessageType::SHUTDOWN,
        VNODE_ID,
        time(&right_fucking_now),
        nlohmann::json::object()
    };

    int err = emit_event(sock, shutdown);

    if (err != 0) {
        log::error("Failed to send shutdown signal - still shutting down VNODE", {log::field("err", err)});
        close(sock);
        return err;
    }

    log::info("Node shutdown sent ... shutting down now");
    close(sock);
    return 0;




}



