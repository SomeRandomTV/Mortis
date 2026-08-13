#include <iostream>
#include <stdexcept>
#include "m0rtis_node.hpp"

using namespace m0rtis;

/**
 * entry point for the m0rtis_node binary - parses HOST/PORT off argv,
 * connects one MortisNode to the hub (VNODE_ID hardcoded to 1 for now,
 * see the recv_loop hardcoding note on the hub side - the two currently
 * have to agree by coincidence, not by design), then runs its event
 * loop until that finishes on its own.
 *
 * usage: ./m0rtis_node <HOST> <PORT>
 *
 * @return 0 on a clean run, -1 wrong arg count, -2 bad port, or
 *         whatever connect_hub() returned if the handshake never went
 *         through
 */
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

    MortisNode vnode(host, port, 1);
    int connection = vnode.connect_hub();
    if (connection != 0) {
        return connection;
    }

    vnode.event_loop();

    return 0;

}
