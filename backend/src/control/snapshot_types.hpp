#pragma once

// 控制模块对外共享的机器人状态快照（SPEC 5.5「已确认快照」）。
// 独立成头文件，供 RobotController 与 PatrolExecutor 共同引用，
// 避免控制层内部循环包含。

#include <string>

#include "domain/types.hpp"

namespace patrol {

// 对外可观察的机器人状态快照。所有字段在互斥保护下复制，
// 调用方拿到的是一份一致的数据副本。
struct RobotSnapshot {
    std::string robotId;
    std::string instanceId;
    Position position;
    Direction heading;
    MotionStatus motionStatus;
    LastResult lastResult;
    int sequence{0};
    bool patrolActive{false};
};

}  // namespace patrol
