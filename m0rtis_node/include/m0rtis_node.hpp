#pragma once 

#include <cstdint>
namespace m0rtis {

    //  Protocol Version
    inline constexpr const char* NODE_VERSION = "0.0.0";

    class MortisNode {
        
        public:

            const char *_HOST;
            unsigned _PORT;
            uint8_t VNODE_ID;
            
            MortisNode(const char *HOST, unsigned PORT, uint8_t ID) : _HOST(HOST), _PORT(PORT), VNODE_ID(ID) {}

            int connect_hub();      // build envelope send to hub  

        private:
            int sock;

    
    };

}   // namespace m0rtis
