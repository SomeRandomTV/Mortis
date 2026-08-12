#include <atomic>
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

        std::cout << "Connection to " << _vnode_id << " accepted\n";
        ConnectedNode valid_node {
            ip_str,                          // node_addr
            true,                            // is_connected
            15,                              // heartbeat timer
            connection_state::CONNECTION_STATES::ACTIVE,       // node_state: AWAITING_HANDSHAKE -> ACTIVE
        };

        connected_nodes[_vnode_id] = valid_node;
        break;
    }

    std::cout << "Connected ... " << std::endl;
    return 0;    

    
}
