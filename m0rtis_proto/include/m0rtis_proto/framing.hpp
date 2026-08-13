#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <optional>
#include <iostream>
#include <chrono>
#include <sys/socket.h>
#include <vector>

#include <nlohmann/json.hpp>
#include <sys/types.h>
#include "envelope.hpp"

// This is the length-prefixed framing layer - the thing that turns "a
// stream of bytes on a TCP socket" into "a stream of whole Envelopes".
// TCP doesn't know or care about message boundaries, so every Envelope
// goes out as a 4-byte network-order length header followed by exactly
// that many bytes of JSON. emit_event/recv_event are the only functions
// hub/node actually call - frame_utils::send_all/recv_all are just the
// send()/recv() retry-loop plumbing underneath, not meant to be called
// directly from outside this file.
//
// Not thread-safe: nothing in here does any locking, so two threads
// calling emit_event on the same socket at the same time can interleave
// their writes and corrupt the framing on the wire. Caller's job to
// serialize access to a given socket if that's ever a thing (see
// MortisNode's eventual heartbeat-thread design).
namespace m0rtis {

    constexpr const int MAX_MSG_SIZE = 1024;

    namespace frame_utils {
        // keeps calling send() until all `nbytes` are out, since a single
        // send() call is never guaranteed to take the whole buffer at once.
        // returns 0 once everything's sent, -1 on a hard socket error, -2
        // if send() reports 0 bytes moved (peer's gone - not "done", it's a
        // failure, same as a negative return)
        inline int send_all(int sock, const char *data, size_t nbytes) {
            size_t nbytes_sent(0);
            while (nbytes_sent < nbytes) {

                ssize_t b = send(sock, data + nbytes_sent, nbytes - nbytes_sent, 0);
                if (b < 0) {
                    perror("send");
                    return -1;

                }
                if (b == 0) {
                    return -2;
                }
                nbytes_sent += static_cast<size_t>(b);
            }
            return 0;
        }

        // same deal as send_all but for recv() - loops until `nbytes` bytes
        // have actually landed in `data`. returns 0 on a full read, -1 on a
        // hard error, -2 if recv() returns 0 (peer closed the connection -
        // EOF, not a successful zero-length read)
        inline int recv_all(int sock, char *data, size_t nbytes) {
            // this file needs to fill the const char data*

            size_t nbytes_recv(0);
            while (nbytes_recv < nbytes) {
                ssize_t b = recv(sock, data + nbytes_recv, nbytes - nbytes_recv, 0);

                if (b < 0) {
                    perror("recv");
                    return -1;
                }
                if (b == 0) {
                    return -2;
                }

                nbytes_recv += static_cast<size_t>(b);
            }
            return 0;

        }

    }

    // serializes `env` to JSON and sends it as one length-prefixed frame:
    // 4-byte network-order length header, then the JSON body itself.
    // rejects (without touching the socket at all) if the serialized body
    // would exceed MAX_MSG_SIZE, so an oversized payload never gets
    // half-sent.
    //
    // return codes: 0 success, -1 message too big (checked before any
    // I/O), -10 hard error sending the length prefix, -20 peer closed
    // while sending the length prefix, -11 hard error sending the body,
    // -21 peer closed while sending the body
    inline int emit_event(int sock, Envelope &env) {
        std::string msg_body = nlohmann::json(env).dump();

        if (msg_body.size() > MAX_MSG_SIZE) {
            std::cerr << "Cannot send message bigger than MAX_MSG_SIZE \n";
            return -1;
        }

        const uint32_t len_net = htonl(static_cast<uint32_t>(msg_body.size()));
        int err = frame_utils::send_all(sock, reinterpret_cast<const char *>(&len_net), sizeof(len_net));      // send length prefix first

        if (err == -1) {
            std::cerr << "Failed to send prefix data ... ";    // followed by perror
            return -10;
        }

        if (err == -2) {
            std::cerr << "Peer connection closed ... \n";
            return -20;
        }

        err = frame_utils::send_all(sock, msg_body.data(), msg_body.size());
        if (err == -1) {
            std::cout << "Failed to send body data ... error: " << err << "\n";
            return -11;
        }

        if (err == -2) {
            std::cerr << "Peer connection closed ... \n";
            return -21;
        }

        std::cout << "Event Successfully Emitted ... \n";
        return 0;

    }



    // reads one length-prefixed frame off `sock` and parses it into an
    // Envelope: 4-byte length header first, then exactly that many bytes
    // of JSON body. Every failure path - short/failed read on the prefix,
    // an oversized declared length, a short/failed read on the body, or a
    // body that doesn't parse as a valid Envelope - comes back as
    // std::nullopt rather than throwing, since a malformed/partial message
    // is an expected condition on a network boundary, not a program bug.
    // Callers are expected to treat nullopt as "this connection is done,
    // close it" rather than something retryable.
    inline std::optional<Envelope> recv_event(int sock) {

        uint32_t len_net{ 0 };
        int err = frame_utils::recv_all(sock, reinterpret_cast<char *>(&len_net), sizeof(len_net));     // receive length prefix first


        if (err == -1) {
            std::cerr << "Failed to receive length prefix ... ";    // followed by perror
            return std::nullopt;

        }
        if (err == -2) {
            std::cout << "Peer connection closed ... \n";
            return std::nullopt;
        }



        const uint32_t len = ntohl(len_net);
        if (len > MAX_MSG_SIZE) {
            std::cerr << "ERROR: MAX MESSAGE SIZE REACHED" << std::endl;
            return std::nullopt;
        }
        std::vector<char> msg(len);

        if (len > 0 && frame_utils::recv_all(sock, msg.data(), len) != 0) {
            std::cerr << "ERROR: Could not receive data" << std::endl;
            return std::nullopt;
        }

    try {
        return nlohmann::json::parse(msg.begin(), msg.end()).get<Envelope>();
    } catch (const std::exception &e) {
        std::cerr << "ERROR: could not parse data into Envelope ..." << std::endl;
        return std::nullopt;

    }


    }



}
