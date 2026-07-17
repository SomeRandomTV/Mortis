#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
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

    sockaddr_in n_addr;
    socklen_t n_addr_len = sizeof(n_addr);
    int _connection = accept(sock, reinterpret_cast<sockaddr *>(&n_addr), &n_addr_len);

    if (_connection == -1) {
        perror("accept");
        close(sock);
        return -5;
    }
    
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &n_addr.sin_addr, ip_str, sizeof(ip_str));
    std::cout << "Client connected from " << ip_str << ":" << ntohs(n_addr.sin_port) << std::endl;


    close(sock);
    std::cout << "Connection Closed ... " << std::endl;
    return 0;



    

    
}
