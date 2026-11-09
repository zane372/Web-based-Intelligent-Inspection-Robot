#pragma once

// 虚拟感知值对象（SPEC 第 6 章）。只依赖 domain 的基础类型，
// 不依赖 Crow / nlohmann/json / Boost，便于作为纯规则库单独测试。

#include <optional>
#include <string>
#include <vector>

#include "domain/types.hpp"

namespace patrol {

// 三向测距命中类型。
enum class HitType { None, Boundary, Obstacle };

// 测距传感器的相对方向。
enum class RelativeDirection { Front, Left, Right };

// 虚拟异常源类型。
enum class HazardType { Smoke, HighTemp, GasLeak };

// 异常严重程度。
enum class Severity { Low, Medium, High };

// 感知事件类型。
enum class PerceptionEventType {
    AnomalyDetected,
    AnomalyOutOfRange,
    ContactBoundary,
    ContactObstacle,
};

// 配置中的虚拟异常源（真值，前端不可见，直到进入感知半径）。
struct Hazard {
    std::string id;
    HazardType type{HazardType::Smoke};
    Severity severity{Severity::Low};
    Position position{0, 0};
    std::string label;
};

// 虚拟感知配置（与 map.json 分离，SPEC 6.2）。未设 PERCEPTION_CONFIG
// 环境变量时 enabled=false；非法配置在启动时抛错、进程非零退出。
struct PerceptionConfig {
    bool enabled{false};
    int rangeCells{5};
    int hazardRangeCells{2};
    std::vector<Hazard> hazards;
};

// 单条三向测距读数。
struct RangeReading {
    std::string sensorId;                 // range-front / range-left / range-right
    RelativeDirection relativeDirection{RelativeDirection::Front};
    Direction absoluteDirection{Direction::Right};
    HitType hitType{HitType::None};
    int distanceCells{0};                 // 命中时有意义
    std::optional<Position> cell;         // OBSTACLE 命中时为目标障碍格
};

// 一条当前检测读数（异常源进入感知半径）。
struct Detection {
    std::string hazardId;
    HazardType hazardType{HazardType::Smoke};
    Severity severity{Severity::Low};
    std::string label;
    Position position{0, 0};
    int distanceCells{0};
};

// 一次观察：测距 + 全部检测，绑定到机器人序列号。
struct PerceptionObservation {
    int sequence{0};
    int rangeCells{0};
    std::vector<RangeReading> ranges;
    std::vector<Detection> detections;
};

// 一条感知事件（独立 eventId，与 sequence 分离）。
struct PerceptionEvent {
    int eventId{0};
    std::string instanceId;
    int sequence{0};
    PerceptionEventType type{PerceptionEventType::AnomalyDetected};
    Position position{0, 0};
    std::optional<Detection> hazard;              // 异常类事件
    std::optional<LastResult> contactResult;      // 接触类事件（BLOCKED_*）
};

}  // namespace patrol
