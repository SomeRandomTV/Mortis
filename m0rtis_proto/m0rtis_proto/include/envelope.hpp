#pragma  once 

#include <cstdint>
namespace m0rtis::envelope {

struct Envelope {
    const char *version;    // ties to the protocol version 
    const char *type;       // type of event emitted 
    uint64_t node_id;       // node id 
    int64_t timestamp;      // timestamp from unix epoch 

}__attribute__((packed));






}   // namespace m0rtis::envelope
