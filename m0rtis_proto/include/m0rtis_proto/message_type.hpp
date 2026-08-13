#pragma once

#include <string>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace m0rtis {

/* Top level Message Envelope
 *  Only 3 Messages types defined for Phase 0
 *      - Handshake -> On first connections
 *      - InferenceEvent -> Event Emitted from the IRIS
 *      - Heartbeat -> Sent from node after a timer from last emitted event
 */
enum class MessageType {
    InferenceEvent,
    Handshake,
    Heartbeat,
    SHUTDOWN,
    UNKNOWN,
};  // these will get serialized into "inference_event" | "handshake" | "heartbeat"

// enum -> wire string. this is the strict side of the strict/lenient
// split - UNKNOWN has no case of its own, it just falls into default
// and throws, same as it would for any MessageType value that somehow
// isn't one of the known ones. don't try to serialize UNKNOWN, it's not
// a real message type, it's what a bad envelope's `type` field decodes
// into on the way in
inline std::string msg_to_string(MessageType t) {
    switch (t) {
        case MessageType::Handshake:
            return "handshake";
        case MessageType::Heartbeat:
            return "heartbeat";
        case MessageType::InferenceEvent:
            return "inference_event";
        case MessageType::SHUTDOWN:
            return "SHUTDOWN";
        case MessageType::UNKNOWN:
        default:
            throw std::runtime_error("ERROR: Unknown Message type. REJECTING .... ");
    }
}

// wire string -> enum. throws on anything that isn't one of the known
// strings - this is the whole point of the strict/lenient split
// (docs/IMPLEMENTATION.md §3): an envelope `type` we don't recognize
// means the two sides are speaking different protocol versions, so we'd
// rather blow up here (and have recv_event() turn that into a nullopt)
// than silently accept garbage. contrast with event_type.hpp's lenient
// fallback-to-UNKNOWN behavior for the payload's event_type - that one's
// allowed to be forward-compatible, this one isn't
inline MessageType string_to_msg(const std::string &msg) {
    if (msg == "handshake") {
        return MessageType::Handshake;
    }
    if (msg == "heartbeat") {
        return MessageType::Heartbeat;
    }
    if (msg == "inference_event") {
        return MessageType::InferenceEvent;
    }
    if (msg == "SHUTDOWN") {
        return MessageType::SHUTDOWN;
    }

    throw std::runtime_error("ERROR: Unknown messagee type " + msg);
}

// nlohmann hook - lets `nlohmann::json(some_message_type)` just work
inline void to_json(nlohmann::json &j, const MessageType &t) {
    j = msg_to_string(t);
}

// nlohmann hook - lets `j.get<MessageType>()` just work. this is the
// thing that actually makes Envelope::from_json's `type` field throw
// on garbage, since it delegates straight to string_to_msg above
inline void from_json(const nlohmann::json &j, MessageType &t) {

    t = string_to_msg(j.get<std::string>());
}



}   // namespace m0rtis
