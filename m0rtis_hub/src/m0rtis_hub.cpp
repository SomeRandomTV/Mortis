#include <atomic>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <chrono>
#include <thread>
#include "m0rtis_hub.hpp"

using namespace m0rtis;

int MortisHub::connect_node() {
    
    std::cout << "HUB serving at " << _HOST << ":" << _PORT << std::endl;
    int sock = socket(AF_INET, SOCK_STREAM, 0);

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
    while (true) {
        sockaddr_in n_addr;
        socklen_t n_addr_len = sizeof(n_addr);
        int _connection = accept(sock, reinterpret_cast<sockaddr *>(&n_addr), &n_addr_len);

        if (_connection == -1) {
            perror("accept");
            continue;
        }
        
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &n_addr.sin_addr, ip_str, sizeof(ip_str));
        std::cout << "Node connected from " << ip_str << ":" << ntohs(n_addr.sin_port) << std::endl;
        
        while (true) {
        
            char buff[1024];
            ssize_t bytes_recv = recv(_connection, buff, sizeof(buff), 0);
            if (bytes_recv < 0) {
                perror("recv");
                close(_connection);
                close(sock);
                return -5;
            }

            if (bytes_recv == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                break;
            }

            buff[bytes_recv] = '\0';
            std::cout << "Received: " << buff << std::endl;
            
            ssize_t bytes_sent = send(_connection, buff, bytes_recv, 0);
            std::cout << "Bytes sent: " << bytes_sent << std::endl;

        }
        std::cout << "Node disconnected" << std::endl;
        close(_connection);
    }

    close(sock);

    std::cout << "Connection Closed ... " << std::endl;
    return 0;



    

    
}
