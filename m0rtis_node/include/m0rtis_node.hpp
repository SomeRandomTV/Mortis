#pragma once 

namespace m0rtis {

    //  Protocol Version
    inline constexpr const char* VERSION = "0.0.0";

    class MortisNode {
        
        public:

            const char *_HOST;
            unsigned _PORT;
            
            MortisNode(const char *HOST, unsigned PORT) : _HOST(HOST), _PORT(PORT) {}

            int connect_hub();
    
    };

}   // namespace m0rtis
