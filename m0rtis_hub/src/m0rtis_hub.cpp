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

void MortisHub::kill_node_connection(uint8_t _id) {
    
    if (connected_nodes.find(_id) == connected_nodes.end()) {
        std::cerr << "ERROR: V-Node does not exist in connected_nodes\n";
    } else {
        std::cout << "Killing connection to node: " << _id;
        close(connected_nodes[_id].node_sock);
    }

}

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
    while (true) {
        std::optional<Envelope> handshake {recv_event(_connection)};
        
        if (handshake == std::nullopt) {
            continue;        
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
        break;
    }

    std::cout << "Connected ... " << std::endl;
    return 0;    

    
}

void MortisHub::recv_loop() {
    
    int counter{0};
    while (counter++ < 5) {
        
        std::optional<Envelope> env = recv_event(connected_nodes[1].node_sock);

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

                std::cout << "Time: " << env->timestamp_ms << "\n";
                std::cout << "From ID: " << env->vnode_id << "\n";
                std::cout << "========== Payload ==========\n";
                std::cout << env->payload.dump(2);
                std::cout << "=============================\n";
                break;
            default:

                std::cerr << "ERROR: Invalid Envelope ... DISCONNECTING \n";
                std::cerr << "Got type: " << msg_to_string(env->type) << "\n";
                kill_node_connection(env->vnode_id); 
                break;

        }
        
    }

    std::cout << "it is done ... ";

}
