#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <ctime>

#include "m0rtis_node.hpp"
#include "m0rtis_proto/envelope.hpp"
#include "m0rtis_proto/framing.hpp"
#include "m0rtis_proto/event_type.hpp"
#include "m0rtis_proto/message_type.hpp"

using namespace m0rtis;


/* =========== Private Methods =========== */
/* ========== Public API Methods ==========*/

int MortisNode::connect_hub() {
    
    std::cout << "Node connecting to Hub at " << _HOST << ":" << _PORT << std::endl;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "ERROR: Failed to create socked" << std::endl;
        close(sock);
        return -1;
    }

    sockaddr_in node_addr{};
    socklen_t node_len = sizeof(node_addr);
    node_addr.sin_family = AF_INET;     // IPv4 192.126.12.123
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
    time_t right_now;
    Envelope env {
       "0.0.0",     // protocol version         
        MessageType::Handshake,
        VNODE_ID,   // vision node id 
        time(&right_now),   // right fucking now 
        nlohmann::json::object()    // empty payload 
        
    };

    int err = emit_event(sock, env);

    if (err != 0) {
        close(sock);
        close(_connection);
        std::cerr << "ERROR: Hub rejected the handshake ... returned " << err << "\n";
        return err;

    }
        
    std::cout << "Hub accepted connection\n";
    return 0;

       
}

void MortisNode::event_loop() {
    
    Envelope inf = next_fixture_inference_event();

    std::cout << "Got Something from the I.R.I.S. ->: \n";
    std::cout << "Type: " << msg_to_string(inf.type) << "\n";
    std::cout << "Time: " << inf.timestamp_ms << "\n";
    std::cout << "From ID: " << inf.version << "\n";
    std::cout << " ======= Payload ======= \n";
    std::cout << inf.payload.dump(2);

    int err = emit_event(sock, inf);

    if (err != 0) {
        std::cout << "Could not send data: " << err << "\n";
        close(sock);
        return;
    }
    
    std::cout << "Envelope sent ... ";

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
        std::cerr << "Could not send heartbeat ... error: " << err << "\n";
        close(sock);
        return;
    }
}



