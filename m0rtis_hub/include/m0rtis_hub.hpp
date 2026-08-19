#pragma once

#include <cstdint>
#include <map>

#include "m0rtis_proto/connection-state.hpp"

// AXIOM's side of the wire - MortisHub is the server: binds/listens,
// accepts one V-Node at a time (multi-threading for concurrent nodes is
// still SCRUM-12, not done yet), enforces handshake-first, then dispatches
// whatever the node sends after that. connected_nodes is the hub's whole
// view of who's out there - keyed by vnode_id, one ConnectedNode per node
// that's made it past the handshake.
namespace m0rtis {



    struct ConnectedNode {
        std::string node_addr;
        bool is_connected;
        // heartbeat time out? decremented timer? reset after inf event | heartbeat?
        unsigned heartbeat_t;     // set to 15 seconds
        connection_state::CONNECTION_STATES node_state;       // hub side view of the node DISCONNECTED | ACTIVE
        int node_sock;
    };

    //  Protocol Version
    inline constexpr const char* HUB_VERSION = "0.0.0";


    class MortisHub {

        public:

            const char *_HOST;
            unsigned _PORT;
            MortisHub(const char *HOST, unsigned PORT) : _HOST(HOST), _PORT(PORT) {}
            int accept_connection();        // waits to get the handshake
            void recv_loop();

        private:
            std::map<uint64_t, ConnectedNode> connected_nodes;
            int sock; 
            int _heartbeat_sock;
            uint8_t _active_vnode_id{ 0 };   // vnode_id of the connection accept_connection() most recently registered

            void kill_node_connection(uint8_t _id);
            void heartbeat_daemon();        // waits for heartbeat 



    };

}   // namespace m0rtis
