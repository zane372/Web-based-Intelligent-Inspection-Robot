#pragma once

#include <functional>
#include <mutex>
#include <string>

#include "control/patrol_executor.hpp"
#include "control/snapshot_types.hpp"
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

// 命令执行结果：结果枚举 + 命令执行后的最新快照。
struct CommandOutcome {
    CommandResult result;
    RobotSnapshot robot;
};

// 控制模块：组合 Grid 与 Robot，维护序列号与线程安全，并把自动巡检的
// 线程与路径游标委托给 PatrolExecutor（SPEC 5.3/5.5 状态所有权）。
// 只有本类负责对 Robot 的修改，保证「一次命令 = 一个原子状态迁移」。
//
// 自动巡检语义：
// - StartPatrol 从当前位置规划一条覆盖路径，交执行器启动后台线程；
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

    // 设置推进间隔（毫秒），主要供测试将间隔调大以确定性驱动。
    void setPatrolIntervalMs(int milliseconds) {
        executor_.setIntervalMs(milliseconds);
    }

    // 设置状态变更通知回调（SPEC 6.5 观察触发）。每次命令执行、每个巡检
    // 步推进后，回调都会收到已确认快照；回调在控制器锁外调用（AR-CON-007），
    // 不得在回调内再获取控制器锁。回调可能来自后台巡检线程。
    void setObserver(std::function<void(const RobotSnapshot&)> observer) {
        observer_ = std::move(observer);
    }

private:
    mutable std::mutex mutex_;
    Grid grid_;
    Robot robot_;
    Position initialPosition_;
    Direction initialHeading_;
    std::string robotId_;
    std::string instanceId_;
    int sequence_{0};

    // 自动巡检线程、条件变量、路径与游标的唯一所有者（SPEC 5.3）。
    PatrolExecutor executor_;

    // 状态变更观察者（虚拟感知服务经此订阅，见 main.cpp 接线）。
    std::function<void(const RobotSnapshot&)> observer_;

    RobotSnapshot buildSnapshotLocked() const;

    // 停止巡检并把线程移出交给调用方 join（调用方须已持有 mutex_）。
    std::thread stopAndTakeThreadLocked();

    // 若巡检进行中则停止并等待线程退出（调用方不得持有 mutex_）。
    void stopPatrolAndJoin();
};

}  // namespace patrol
