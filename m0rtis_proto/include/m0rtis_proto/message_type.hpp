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
    UNKNOWN,
};  // these will get serialized into "inference_event" | "handshake" | "heartbeat"

inline std::string msg_to_string(MessageType t) {
    switch (t) {
        case MessageType::Handshake:
            return "handshake";
        case MessageType::Heartbeat:
            return "heartbeat";
        case MessageType::InferenceEvent:
            return "inference_event";
    }
    
    throw std::runtime_error("ERROR: Unknown MessageType");

}

inline MessageType string_to_msg(std::string &msg) {
    if (msg == "handshake") {
        return MessageType::Handshake;
    }
    if (msg == "heartbeat") {
        return MessageType::Heartbeat;
    }
    if (msg == "inference_event") {
        return MessageType::InferenceEvent;
    }

    throw std::runtime_error("ERROR: Unknown messagee type " + msg);
}

inline void to_json(nlohmann::json &j, const MessageType &t) {
    j = msg_to_string(t);
}

inline void from_json(nlohmann::json &j, const MessageType &t) {

    j = string_to_msg(j.get<std::string>());
}



}   // namespace m0rtis 
