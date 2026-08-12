#pragma once

#include <cstdint>
#include <map>

#include "m0rtis_proto/connection-state.hpp"

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

            void kill_node_connection(uint8_t _id);


            
    };

}   // namespace m0rtis
