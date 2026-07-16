#include <iostream>
#include <stdexcept>
#include "m0rtis_node.hpp"

using namespace m0rtis;

int main(int argc, char **argv) {
    
    // ./vnode <HOST> <PORT>
    
    if (argc != 3) {
        std::cerr << "ERROR: Expected 3 arguments but got " << argc << std::endl;
        std::cerr << "USAGE: ./vnode <HOST> <PORT>" << std::endl;
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

    MortisNode vnode(host, port);
    vnode.connect_hub();

    return 0;

}
