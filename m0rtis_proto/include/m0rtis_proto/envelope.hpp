#pragma  once 

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>
#include "m0rtis_proto/message_type.hpp"

namespace m0rtis {



inline constexpr const char *PROTOCOL_VERSION = "0.0.0";

struct Envelope {
    std::string version{ PROTOCOL_VERSION };      // ties to the protocol version 
    MessageType type{ MessageType::UNKNOWN };     // type of event emitted 
    uint8_t vnode_id{ 0 };            // node id 
    int64_t timestamp_ms{ 0 };           // timestamp(ms) from unix epoch 
    nlohmann::json payload{ nlohmann::json::object() };
};


inline void to_json(nlohmann::json &j, const Envelope &env) {
    j = nlohmann::json{
        {"version",   env.version},
        {"type",      env.type},
        {"vnode_id",  env.vnode_id},
        {"timestamp", env.timestamp_ms},
        {"payload",   env.payload},
    };
}

inline void from_json(const nlohmann::json &j, Envelope &env) {

    j.at("version").get_to(env.version);
    j.at("type").get_to(env.type);
    j.at("vnode_id").get_to(env.vnode_id);
    j.at("timestamp").get_to(env.timestamp_ms);
 

    env.payload = j.value("payload", nlohmann::json::object());
}


}   // namespace m0rtis
