#pragma once

// 边界层：C++ 枚举 <-> 大写蛇形字符串，以及对外 JSON DTO。
// 集中在这一层，避免 domain 模块依赖字符串或 JSON。
#include <string>

#include <nlohmann/json.hpp>

#include "control/robot_controller.hpp"
#include "domain/types.hpp"

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
        case LastResult::Reset:           return "RESET";
    }
    return "NONE";
}

inline std::string commandResultToString(CommandResult r) {
    switch (r) {
        case CommandResult::Moved:           return "MOVED";
        case CommandResult::BlockedBoundary: return "BLOCKED_BOUNDARY";
        case CommandResult::Reset:           return "RESET";
    }
    return "MOVED";
}

// GET /api/v1/map 响应体。
inline nlohmann::json mapInfoToJson(const std::string& mapId, int width, int height) {
    return nlohmann::json{
        {"mapId", mapId},
        {"width", width},
        {"height", height},
        {"origin", "TOP_LEFT"},
        {"xAxis", "RIGHT"},
        {"yAxis", "DOWN"},
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

}  // namespace patrol
