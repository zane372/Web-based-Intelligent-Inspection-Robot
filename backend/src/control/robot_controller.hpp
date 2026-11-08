#pragma once

#include <mutex>
#include <string>

#include "domain/grid.hpp"
#include "domain/robot.hpp"

namespace patrol {

// 用户可提交的命令类型。
enum class CommandType {
    Move,
    Reset,
};

// 一次命令的最终结果（与 API 层 result 枚举一一对应）。
enum class CommandResult {
    Moved,
    BlockedBoundary,
    Reset,
};

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
};

// 命令执行结果：结果枚举 + 命令执行后的最新快照。
struct CommandOutcome {
    CommandResult result;
    RobotSnapshot robot;
};

// 控制模块：组合 Grid 与 Robot，维护序列号与线程安全。
// 只有本类负责对 Robot 的修改，保证「一次命令 = 一个原子状态迁移」。
class RobotController {
public:
    // grid 为运行时只读地图；robot 由配置初始位姿构造；
    // robotId/instanceId 为静态标识与进程级实例标识。
    RobotController(Grid grid, Robot robot,
                    std::string robotId, std::string instanceId);

    // 返回当前状态的一致快照（只读，不改变任何状态）。
    RobotSnapshot snapshot() const;

    // 执行一条命令并返回结果与最新快照。
    // - Move：调用 Robot::applyManualMove，sequence 自增 1。
    // - Reset：调用 Robot::reset 恢复初始位姿，sequence 自增 1。
    CommandOutcome execute(CommandType type, Direction direction);

private:
    mutable std::mutex mutex_;
    Grid grid_;
    Robot robot_;
    Position initialPosition_;
    Direction initialHeading_;
    std::string robotId_;
    std::string instanceId_;
    int sequence_{0};

    RobotSnapshot buildSnapshotLocked() const;
};

}  // namespace patrol
