#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "domain/grid.hpp"
#include "domain/robot.hpp"

namespace patrol {

// 用户可提交的命令类型。
enum class CommandType {
    Move,
    Reset,
    StartPatrol,
    StopPatrol,
};

// 一次命令的最终结果（与 API 层 result 枚举一一对应）。
enum class CommandResult {
    Moved,
    BlockedBoundary,
    BlockedObstacle,
    Reset,
    PatrolStarted,
    PatrolStopped,
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
    bool patrolActive{false};
};

// 命令执行结果：结果枚举 + 命令执行后的最新快照。
struct CommandOutcome {
    CommandResult result;
    RobotSnapshot robot;
};

// 控制模块：组合 Grid 与 Robot，维护序列号、自动巡检与线程安全。
// 只有本类负责对 Robot 的修改，保证「一次命令 = 一个原子状态迁移」。
//
// 自动巡检语义：
// - StartPatrol 从当前位置规划一条覆盖路径，并启动后台线程按固定间隔推进；
// - 推进期间每次移动一格，sequence 随之自增；
// - 遍历完成后自动停止（lastResult = PatrolCompleted）；
// - 任何手动命令（Move/Reset）会先隐式停止进行中的巡检再执行。
class RobotController {
public:
    // grid 为运行时只读地图；robot 由配置初始位姿构造；
    // robotId/instanceId 为静态标识与进程级实例标识；
    // patrolIntervalMs 为巡检推进间隔（毫秒，默认 250，测试可调大）。
    RobotController(Grid grid, Robot robot,
                    std::string robotId, std::string instanceId,
                    int patrolIntervalMs = 250);

    // 停止巡检线程并等待退出（若存在）。
    ~RobotController();

    // 不可拷贝；依赖 C++17 保证复制省略，构造处无需移动。
    RobotController(const RobotController&) = delete;
    RobotController& operator=(const RobotController&) = delete;

    // 返回当前状态的一致快照（只读，不改变任何状态）。
    RobotSnapshot snapshot() const;

    // 执行一条命令并返回结果与最新快照。
    // - Move：调用 Robot::applyManualMove，sequence 自增 1。
    // - Reset：调用 Robot::reset 恢复初始位姿，sequence 自增 1。
    // - StartPatrol：规划路径并启动巡检线程，sequence 自增 1。
    // - StopPatrol：停止巡检线程，sequence 自增 1（已停止则空操作）。
    CommandOutcome execute(CommandType type, Direction direction);

    // 推进一步巡检（供测试直接驱动，避免依赖后台线程时序）；
    // 返回是否仍在巡检中。线程安全。
    bool patrolStep();

    // 设置巡检推进间隔（毫秒），主要供测试将间隔调大以确定性驱动。
    void setPatrolIntervalMs(int milliseconds) {
        patrolIntervalMs_ = std::chrono::milliseconds(milliseconds);
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable patrolCv_;
    Grid grid_;
    Robot robot_;
    Position initialPosition_;
    Direction initialHeading_;
    std::string robotId_;
    std::string instanceId_;
    int sequence_{0};

    // 巡检状态。
    bool patrolActive_{false};
    std::vector<Direction> patrolPath_;
    std::size_t patrolIndex_{0};
    std::thread patrolThread_;
    std::chrono::milliseconds patrolIntervalMs_{250};

    RobotSnapshot buildSnapshotLocked() const;

    // 推进一格（调用方需已持有 mutex_）；返回是否仍应继续巡检。
    bool patrolStepLocked();

    // 巡检线程体：按间隔推进，直到被停止或遍历完成。
    void patrolLoop();

    // 若巡检进行中则停止并等待线程退出（调用方不得持有 mutex_）。
    void stopPatrolAndJoin();

    // 置停巡检并把线程移出交给调用方 join（调用方须已持有 mutex_）。
    std::thread takePatrolThreadLocked();
};

}  // namespace patrol
