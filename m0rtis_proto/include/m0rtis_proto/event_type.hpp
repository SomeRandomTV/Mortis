#pragma once
#include <nlohmann/json.hpp>

// TODO - Wire this into the payload of a message - im thinking at the frame level 
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

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
    {EventType::UNKNOWN,        "UNKNOWN"},
    {EventType::PersonDetected, "person_detected"},
    {EventType::MotionDetected, "motion_detected"},
    {EventType::ObjectDetected, "object_detected"},
    {EventType::HazardDetected, "hazard_detected"},
    {EventType::ZoneEntered,    "zone_entered"},
    {EventType::ZoneExited,     "zone_exited"},
});


struct BoundingBox {
    int x, y, w, h;
};

struct InferenceEvent {
    EventType type;
    double confidence;
    BoundingBox bbox;
    nlohmann::json metadata;

};
 

}   // namespace m0rtis 
