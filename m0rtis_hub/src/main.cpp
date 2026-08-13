#include <iostream>
#include <stdexcept>
#include "m0rtis_hub.hpp"

using namespace m0rtis;

/**
 * entry point for the m0rtis_hub binary - parses HOST/PORT off argv,
 * spins up one MortisHub, waits for a single V-Node to connect + handshake,
 * then services that connection until it ends. process exits once
 * recv_loop() returns - doesn't go back for a second node yet (SCRUM-19)
 *
 * usage: ./m0rtis_hub <HOST> <PORT>
 *
 * @return 0 on a clean run, -1 wrong arg count, -2 bad port, -10 the
 *         node never got past accept_connection()
 */
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

    m_hub.recv_loop();


    return 0;

}
