#pragma once 

namespace m0rtis {

enum class EventType {
    UNKNOWN,
    PersonDetected, 
    MotionDetected, 
    ObjectDetected, 
    HazardDetected, 
    ZoneEntered,
    ZoneExited

}

NLOHMANN_JSON_SERIALIZE_ENUM(EventType, {
    {EventType::Unknown,        "unknown"},
    {EventType::PersonDetected, "person_detected"},
    {EventType::MotionDetected, "motion_detected"},
    {EventType::ObjectDetected, "object_detected"},
    {EventType::ZoneEntered,    "zone_entered"},
    {EventType::ZoneExited,     "zone_exited"},
})
 

}   // namespace m0rtis 
