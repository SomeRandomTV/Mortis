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

namespace m0rtis {

    constexpr const int MAX_MSG_SIZE = 1024;
    
    namespace frame_utils {
        inline int send_all(int sock, const char *data, size_t nbytes) {
            size_t nbytes_sent(0);
            while (nbytes_sent < nbytes) {
    
                ssize_t b = send(sock, data + nbytes_sent, nbytes - nbytes_sent, 0);
                if (b < 0) {
                    perror("send");
                    return -1;
                    
                }
                if (b == 0) {
                    std::cout << "All data sent ..." << std::endl;
                    break;
                    
                }
                nbytes_sent += static_cast<size_t>(b);
            }

            return 0;
        }

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
                    std::cout << "Connection closed ..." << std::endl;
                    return -2;
                }

                nbytes_recv += static_cast<size_t>(b);
            }
            return 0;

        }

    }
    
    inline int emit_event(int sock, Envelope &env) {
        std::string msg_body = nlohmann::json(env).dump();
    
        if (msg_body.size() > MAX_MSG_SIZE) {
            std::cerr << "ERROR: MAX MESSAGE SIZE REACHED" << std::endl;
            return -1;
        }

        const uint32_t len_net = htonl(static_cast<uint32_t>(msg_body.size()));   
        int err = frame_utils::send_all(sock, reinterpret_cast<const char *>(&len_net), sizeof(len_net));      // send length prefix first 

        if (err != 0) {
            std::cout << "Shiiiii failed to send length prefix ..." << std::endl;
            return -2;
        }

        err = frame_utils::send_all(sock, msg_body.data(), msg_body.size());
        if (err != 0) {
            std::cout << "fuhhhhh mijo, failed to send body data ... " << std::endl;
            return -3;
        }

        std::cout << "Event Successfully Emitted ... " << std::endl;
        return 0;
        
    }

    

    inline std::optional<Envelope> recv_event(int sock) {
        
        uint32_t len_net(0);
        int err = frame_utils::recv_all(sock, reinterpret_cast<char *>(&len_net), sizeof(len_net));     // receive length prefix first

        if (err != 0) {
            std::cout << "Receiving length prefix fucking failed ... error: " << err  << std::endl;
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
