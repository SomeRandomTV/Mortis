#pragma once
#include <nlohmann/json.hpp>

// TODO - Wire this into the payload of a message - im thinking at the frame level
//
// This one's the lenient half of the strict/lenient split (see
// message_type.hpp for the strict half). event_type lives inside an
// inference_event envelope's payload, and unlike the envelope's own
// `type`, an event_type we don't recognize should NOT kill the
// connection - it just means a node's running newer firmware than this
// build of AXIOM knows about yet, so it falls back to UNKNOWN instead of
// throwing. That's why UNKNOWN has to stay first in the
// NLOHMANN_JSON_SERIALIZE_ENUM list below - that macro falls back to
// whichever pair is listed first on an unmatched string, it's not a
// coincidence.
namespace m0rtis {

enum class EventType {
    UNKNOWN,
    PersonDetected,
    MotionDetected,
    ObjectDetected,
    HazardDetected,
    ZoneEntered,
    ZoneExited

};

// generates to_json/from_json for EventType in one shot - this is what
// gives us the lenient fallback for free, nlohmann's macro already does
// exactly what we want here (unmatched string -> first pair -> UNKNOWN)
// so there's no hand-written version like message_type.hpp has
NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
    {EventType::UNKNOWN,        "UNKNOWN"},
    {EventType::PersonDetected, "person_detected"},
    {EventType::MotionDetected, "motion_detected"},
    {EventType::ObjectDetected, "object_detected"},
    {EventType::HazardDetected, "hazard_detected"},
    {EventType::ZoneEntered,    "zone_entered"},
    {EventType::ZoneExited,     "zone_exited"},
});


// matches docs/MESSAGES.md's inference_event payload's bounding_box shape.
// no to_json/from_json written for this yet - not wired into
// Envelope::payload anywhere, still just a plain struct (SCRUM-15)
struct BoundingBox {
    int x, y, w, h;
};

// same story - meant to be the typed stand-in for a raw inference_event
// payload (event_type + confidence + bbox + open-ended metadata) but
// there's no to_json/from_json for it yet either, so nothing can
// actually do `payload.get<InferenceEvent>()` right now. today the
// payload's still just handled as opaque nlohmann::json wherever it's
// read (see m0rtis_hub.cpp's recv_loop)
struct InferenceEvent {
    EventType type;
    double confidence;
    BoundingBox bbox;
    nlohmann::json metadata;

};


}   // namespace m0rtis
