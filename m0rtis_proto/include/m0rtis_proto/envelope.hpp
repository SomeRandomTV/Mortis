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

// The envelope itself. Deliberately dumb/flat - no inheritance, no
// per-type subclassing, just one struct with a json blob (`payload`)
// whose shape depends on `type`. Keeps to_json/from_json simple since
// there's only ever one shape to (de)serialize at this layer.
struct Envelope {
    std::string version{ PROTOCOL_VERSION };      // ties to the protocol version
    MessageType type{ MessageType::UNKNOWN };     // type of event emitted
    uint8_t vnode_id{ 0 };            // node id
    int64_t timestamp_ms{ 0 };           // timestamp(ms) from unix epoch
    nlohmann::json payload{ nlohmann::json::object() };
};


// serializes an Envelope to wire-format JSON - nlohmann finds this via
// ADL whenever something does `nlohmann::json(env)` or `.dump()`s an
// Envelope, so callers never call this directly
inline void to_json(nlohmann::json &j, const Envelope &env) {
    j = nlohmann::json{
        {"version",   env.version},
        {"type",      env.type},
        {"vnode_id",  env.vnode_id},
        {"timestamp", env.timestamp_ms},
        {"payload",   env.payload},
    };
}

// parses wire-format JSON back into an Envelope - again, found via ADL
// through `j.get<Envelope>()`. every field except payload is required
// (`.at()`, not `.value()`) so a message missing one of them throws
// instead of silently defaulting - that's what makes a malformed/partial
// message come back as recv_event() returning nullopt instead of an
// Envelope full of zeros. payload alone defaults to an empty object since
// e.g. heartbeat's payload is legitimately just `{}`
inline void from_json(const nlohmann::json &j, Envelope &env) {

    j.at("version").get_to(env.version);
    j.at("type").get_to(env.type);
    j.at("vnode_id").get_to(env.vnode_id);
    j.at("timestamp").get_to(env.timestamp_ms);


    env.payload = j.value("payload", nlohmann::json::object());
}


}   // namespace m0rtis
