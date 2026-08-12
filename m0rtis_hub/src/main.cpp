#include <iostream>
#include <stdexcept>
#include "m0rtis_hub.hpp"

using namespace m0rtis;

int main(int argc, char **argv) {
    
    // ./vnode <HOST> <PORT>
    
    if (argc != 3) {
        std::cerr << "ERROR: Expected 3 arguments but got " << argc << std::endl;
        std::cerr << "USAGE: ./m_hub <HOST> <PORT>" << std::endl;
        return -1;
    }

    const char *host = argv[1];
    unsigned port;
    try {
        
        int p = std::atoi(argv[2]);
        port = static_cast<unsigned>(p);


    } catch (std::out_of_range &e) {
        std::cerr << "ERROR: Failed to convert port from string to unsigned" << std::endl;
        return -2;
    }

    MortisHub m_hub(host, port);
    if (m_hub.accept_connection() != 0) {
        std::cout << "Disconnecting Node ..." << std::endl;
        return -10;
    }
    
    return 0;

}
