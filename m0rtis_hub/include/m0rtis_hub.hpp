#pragma once
#include <map>


namespace m0rtis {


    struct ConnectedNode {
        bool is_connected;
        // heartbeat time out? decremented timer? reset after inf event | heartbeat? 
        unsigned heartbeat_t;
    };

    //  Protocol Version
    inline constexpr const char* VERSION = "0.0.0";


    class MortisHub {
        
        public:

            const char *_HOST;
            unsigned _PORT;
            
            MortisHub(const char *HOST, unsigned PORT) : _HOST(HOST), _PORT(PORT) {}

            int connect_node();     // opens the socket and will wait for handshake       
        
        private:
            std::map<int, ConnectedNode> connected_nodes;


            
    };

}   // namespace m0rtis
