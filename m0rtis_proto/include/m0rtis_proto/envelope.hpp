#pragma  once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>
#include "m0rtis_proto/message_type.hpp"

// This is the one struct every message on the wire gets wrapped in, no
// matter which side sent it or what it's carrying. Hub and node both
// #include this so they're always agreeing on the same shape - if this
// drifts between the two builds we've got a protocol mismatch, not just
// a bug.
//
// Wire shape is flat JSON: version/type/vnode_id/timestamp/payload. Only
// `payload` changes meaning depending on `type` - everything else here
// is always present, always required, no optional fields. See
// docs/MESSAGES.md for the full schema writeup.
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
