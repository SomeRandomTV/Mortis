#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

#include "m0rtis_node.hpp"
#include "m0rtis_proto/framing.hpp"

using namespace m0rtis;

int MortisNode::connect_hub() {
    
    std::cout << "Node connecting to Hub at " << _HOST << ":" << _PORT << std::endl;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "ERROR: Failed to create socked" << std::endl;
        close(sock);
        return -1;
    }

    sockaddr_in node_addr{};
    socklen_t node_len = sizeof(node_addr);
    node_addr.sin_family = AF_INET;
    node_addr.sin_port = htons(_PORT);

    int _c = inet_pton(AF_INET, _HOST, &node_addr.sin_addr);
    if (_c < 1) {
        std::cerr << "ERROR: Failed to convert host IP from string to binary" << std::endl;
        close(sock);
        return -2;
    }

    int _connection = connect(sock, reinterpret_cast<sockaddr *>(&node_addr), node_len);
    if (_connection < 0) {
        perror("connect");
        close(sock);
        return -3;
    }

    std::string msg;

    while(true) {
        std::cout << "m0rtis_node$ ";
        std::getline(std::cin, msg);

        if (msg == "q" || msg == "quit" || msg == "exit") {
            std::cout << "exiting ... ";
            break;
        }


        std::cout << "Sending data ..." << std::endl;
        
        ssize_t bytes_sent = send(sock, msg.c_str(), msg.size(), 0);
        if (bytes_sent < 0) {
            perror("send");
            close(sock);
            return -4;
        } else if (bytes_sent == 0) {
            std::cout << "Done ..." << std:: endl;
            break;
        }
        std::cout << "sdfg" << std::endl;
        
        char buff[1024];
        ssize_t bytes_recv = recv(sock, buff, sizeof(buff) - 1, 0);
        if (bytes_recv < 0) {
            perror("recv");
            close(sock);
            return -5;
        }
        
        buff[bytes_recv] = '\0';
        std::cout << "ECHO => " << buff << std::endl;
    

    }

    close(sock);
    std::cout << "Disconnection from Hub ... " << std::endl;
    return 0;
    
}
