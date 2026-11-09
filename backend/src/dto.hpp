#pragma once

// 边界层：C++ 枚举 <-> 大写蛇形字符串，以及对外 JSON DTO。
// 集中在这一层，避免 domain 模块依赖字符串或 JSON。
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "control/robot_controller.hpp"
#include "domain/types.hpp"
#include "perception/perception_service.hpp"

namespace patrol {

inline std::string directionToString(Direction d) {
    switch (d) {
        case Direction::Up:    return "UP";
        case Direction::Down:  return "DOWN";
        case Direction::Left:  return "LEFT";
        case Direction::Right: return "RIGHT";
    }
    return "RIGHT";
}

inline bool directionFromString(const std::string& s, Direction& out) {
    if (s == "UP")    { out = Direction::Up;    return true; }
    if (s == "DOWN")  { out = Direction::Down;  return true; }
    if (s == "LEFT")  { out = Direction::Left;  return true; }
    if (s == "RIGHT") { out = Direction::Right; return true; }
    return false;
}

inline std::string motionStatusToString(MotionStatus m) {
    switch (m) {
        case MotionStatus::Idle:    return "IDLE";
        case MotionStatus::Blocked: return "BLOCKED";
    }
    return "IDLE";
}

inline std::string lastResultToString(LastResult r) {
    switch (r) {
        case LastResult::None:            return "NONE";
        case LastResult::Moved:           return "MOVED";
        case LastResult::BlockedBoundary: return "BLOCKED_BOUNDARY";
        case LastResult::BlockedObstacle: return "BLOCKED_OBSTACLE";
        case LastResult::Reset:           return "RESET";
        case LastResult::PatrolStarted:   return "PATROL_STARTED";
        case LastResult::PatrolStopped:   return "PATROL_STOPPED";
        case LastResult::PatrolCompleted: return "PATROL_COMPLETED";
    }
    return "NONE";
}

inline std::string commandResultToString(CommandResult r) {
    switch (r) {
        case CommandResult::Moved:           return "MOVED";
        case CommandResult::BlockedBoundary: return "BLOCKED_BOUNDARY";
        case CommandResult::BlockedObstacle: return "BLOCKED_OBSTACLE";
        case CommandResult::Reset:           return "RESET";
        case CommandResult::PatrolStarted:   return "PATROL_STARTED";
        case CommandResult::PatrolStopped:   return "PATROL_STOPPED";
    }
    return "MOVED";
}

inline std::string commandTypeToString(CommandType t) {
    switch (t) {
        case CommandType::Move:       return "MOVE";
        case CommandType::Reset:      return "RESET";
        case CommandType::StartPatrol: return "START_PATROL";
        case CommandType::StopPatrol:  return "STOP_PATROL";
    }
    return "MOVE";
}

// GET /api/v1/map 响应体。
inline nlohmann::json mapInfoToJson(const std::string& mapId, int width, int height,
                                    const std::vector<Position>& obstacles) {
    nlohmann::json arr = nlohmann::json::array();
    for (const Position p : obstacles) {
        arr.push_back({{"x", p.x}, {"y", p.y}});
    }
    return nlohmann::json{
        {"mapId", mapId},
        {"width", width},
        {"height", height},
        {"origin", "TOP_LEFT"},
        {"xAxis", "RIGHT"},
        {"yAxis", "DOWN"},
        {"obstacles", arr},
    };
}

// GET /api/v1/robot 及命令响应中的 robot 字段。
inline nlohmann::json robotSnapshotToJson(const RobotSnapshot& s) {
    return nlohmann::json{
        {"robotId", s.robotId},
        {"instanceId", s.instanceId},
        {"position", {{"x", s.position.x}, {"y", s.position.y}}},
        {"heading", directionToString(s.heading)},
        {"motionStatus", motionStatusToString(s.motionStatus)},
        {"lastResult", lastResultToString(s.lastResult)},
        {"sequence", s.sequence},
        {"patrolActive", s.patrolActive},
    };
}

// 统一错误响应体。
inline nlohmann::json errorToJson(const std::string& code,
                                  const std::string& message,
                                  const std::string& requestId,
                                  const nlohmann::json& details = nlohmann::json::object()) {
    nlohmann::json err = {
        {"code", code},
        {"message", message},
        {"requestId", requestId},
    };
    if (!details.empty()) {
        err["details"] = details;
    }
    return nlohmann::json{{"error", err}};
}

// ---- 虚拟感知序列化（SPEC 6.7）----

inline std::string hitTypeToString(HitType t) {
    switch (t) {
        case HitType::None:     return "NONE";
        case HitType::Boundary: return "BOUNDARY";
        case HitType::Obstacle: return "OBSTACLE";
    }
    return "NONE";
}

inline std::string relativeDirectionToString(RelativeDirection d) {
    switch (d) {
        case RelativeDirection::Front: return "FRONT";
        case RelativeDirection::Left:  return "LEFT";
        case RelativeDirection::Right: return "RIGHT";
    }
    return "FRONT";
}

inline std::string hazardTypeToString(HazardType t) {
    switch (t) {
        case HazardType::Smoke:    return "SMOKE";
        case HazardType::HighTemp: return "HIGH_TEMP";
        case HazardType::GasLeak:  return "GAS_LEAK";
    }
    return "SMOKE";
}

inline std::string severityToString(Severity s) {
    switch (s) {
        case Severity::Low:    return "LOW";
        case Severity::Medium: return "MEDIUM";
        case Severity::High:   return "HIGH";
    }
    return "LOW";
}

inline std::string perceptionEventTypeToString(PerceptionEventType t) {
    switch (t) {
        case PerceptionEventType::AnomalyDetected:    return "ANOMALY_DETECTED";
        case PerceptionEventType::AnomalyOutOfRange:  return "ANOMALY_OUT_OF_RANGE";
        case PerceptionEventType::ContactBoundary:    return "CONTACT_BOUNDARY";
        case PerceptionEventType::ContactObstacle:    return "CONTACT_OBSTACLE";
    }
    return "ANOMALY_DETECTED";
}

inline nlohmann::json positionToJson(Position p) {
    return nlohmann::json{{"x", p.x}, {"y", p.y}};
}

inline nlohmann::json rangeReadingToJson(const RangeReading& r) {
    nlohmann::json j = {
        {"sensorId", r.sensorId},
        {"relativeDirection", relativeDirectionToString(r.relativeDirection)},
        {"absoluteDirection", directionToString(r.absoluteDirection)},
        {"hitType", hitTypeToString(r.hitType)},
        {"distanceCells", nullptr},
        {"cell", nullptr},
    };
    if (r.hitType != HitType::None) {
        j["distanceCells"] = r.distanceCells;
    }
    if (r.cell.has_value()) {
        j["cell"] = positionToJson(*r.cell);
    }
    return j;
}

inline nlohmann::json detectionToJson(const Detection& d) {
    return nlohmann::json{
        {"hazardId", d.hazardId},
        {"hazardType", hazardTypeToString(d.hazardType)},
        {"severity", severityToString(d.severity)},
        {"label", d.label},
        {"position", positionToJson(d.position)},
        {"distanceCells", d.distanceCells},
    };
}

inline nlohmann::json observationToJson(const PerceptionObservation& o) {
    nlohmann::json ranges = nlohmann::json::array();
    for (const RangeReading& r : o.ranges) {
        ranges.push_back(rangeReadingToJson(r));
    }
    nlohmann::json detections = nlohmann::json::array();
    for (const Detection& d : o.detections) {
        detections.push_back(detectionToJson(d));
    }
    return nlohmann::json{
        {"sequence", o.sequence},
        {"rangeCells", o.rangeCells},
        {"ranges", std::move(ranges)},
        {"detections", std::move(detections)},
    };
}

inline nlohmann::json perceptionEventToJson(const PerceptionEvent& e) {
    nlohmann::json j = {
        {"eventId", e.eventId},
        {"instanceId", e.instanceId},
        {"sequence", e.sequence},
        {"type", perceptionEventTypeToString(e.type)},
        {"position", positionToJson(e.position)},
    };
    // 不适用字段省略而非输出 null（SPEC 6.7，审查 M3）。
    if (e.hazard.has_value()) {
        j["hazard"] = detectionToJson(*e.hazard);
    }
    if (e.contactResult.has_value()) {
        j["contactResult"] = lastResultToString(*e.contactResult);
    }
    return j;
}

inline nlohmann::json perceptionLatestToJson(const PerceptionLatestView& v) {
    nlohmann::json active = nlohmann::json::array();
    for (const Detection& d : v.activeDetections) {
        active.push_back(detectionToJson(d));
    }
    nlohmann::json j = {
        {"enabled", v.enabled},
        {"instanceId", v.instanceId},
        {"observation", nullptr},
        {"activeDetections", std::move(active)},
        {"latestEventId", v.latestEventId},
    };
    if (v.observation.has_value()) {
        j["observation"] = observationToJson(*v.observation);
    }
    return j;
}

inline nlohmann::json perceptionEventsToJson(const PerceptionEventsView& v) {
    nlohmann::json events = nlohmann::json::array();
    for (const PerceptionEvent& e : v.events) {
        events.push_back(perceptionEventToJson(e));
    }
    return nlohmann::json{
        {"instanceId", v.instanceId},
        {"events", std::move(events)},
        {"oldestEventId", v.oldestEventId == 0 ? nlohmann::json(nullptr)
                                               : nlohmann::json(v.oldestEventId)},
        {"latestEventId", v.latestEventId},
        {"hasMore", v.hasMore},
        {"truncated", v.truncated},
    };
}

}  // namespace patrol
