#pragma once 

namespace m0rtis {

    //  Protocol Version
    inline constexpr const char* VERSION = "0.0.0";


    class MortisHub {
        
        public:

            const char *_HOST;
            unsigned _PORT;
            
            MortisHub(const char *HOST, unsigned PORT) : _HOST(HOST), _PORT(PORT) {}

            void connect_node();
    
    };

}   // namespace m0rtis
