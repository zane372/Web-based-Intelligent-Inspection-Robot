#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "control/snapshot_types.hpp"
#include "domain/grid.hpp"
#include "domain/robot.hpp"

namespace patrol {

// 自动巡检执行组件：唯一拥有巡检线程、条件变量、停止通知，
// 以及巡检活动标志、路径与游标（SPEC 5.3 状态所有权）。
//
// 并发约定（SPEC 5.5 AR-CON-001/004/006）：
// - 所有状态变更都发生在调用方提供的互斥量（RobotController 的机器人锁）
//   保护下；本类自身不持有或获取任何互斥量，避免与控制锁嵌套。
// - 线程体 `loop` 只在该互斥量上做 `wait_for`，醒来后释放锁推进。
// - 停止路径：先置 active=false 并通知条件变量，把线程移出，
//   由调用方在锁外 join（AR-CON-004）。
class PatrolExecutor {
public:
    PatrolExecutor() = default;

    // 若线程仍 joinable，则与 std::thread 一致触发 terminate；
    // 正常流程由 RobotController 析构先调用 stopAndTakeThread 并 join。
    ~PatrolExecutor() = default;

    PatrolExecutor(const PatrolExecutor&) = delete;
    PatrolExecutor& operator=(const PatrolExecutor&) = delete;

    // 是否正在巡检（调用方须持有控制器锁）。
    bool active() const { return active_; }

    // 以给定路径启动巡检并拉起后台线程。
    // 调用方须持有控制器锁；robot/grid/mutex 为控制器成员引用，
    // 生命周期由控制器保证覆盖巡检线程。sequence 为控制器序列号引用，
    // 每个巡检步在其上 +1（与命令共用同一单调序列）。
    void start(std::vector<Direction> path, std::mutex& mutex, Robot& robot,
               const Grid& grid, int& sequence);

    // 置停巡检并返回待 join 的线程（可能为空）。
    // 调用方须持有控制器锁，且必须在锁外 join 返回的线程。
    std::thread stopAndTakeThread();

    // 推进一步巡检（含必要时在锁外重规划）。调用方须持有控制器锁；
    // 返回是否仍应继续巡检。步骤内会短暂释放锁做全图规划（AR-CON-002/003）。
    // 每推进/受阻一步都会对 sequence +1。
    bool stepReplanned(std::unique_lock<std::mutex>& lock, Robot& robot,
                       const Grid& grid, int& sequence);

    // 设置推进间隔（毫秒），供测试调大以确定性驱动。
    void setIntervalMs(int milliseconds) {
        intervalMs_.store(std::chrono::milliseconds(milliseconds));
    }

    // 设置快照标识（robotId/instanceId），供 buildSnapshotLocked 使用。
    // 由 RobotController 构造时调用一次。
    void setIdentity(std::string robotId, std::string instanceId);

    // 设置「每个巡检步完成后的通知回调」。回调以已确认快照为参数，
    // 在控制器锁外调用（AR-CON-006/007）。
    void setOnStep(std::function<void(const RobotSnapshot&)> callback);

private:
    void loop(std::mutex& mutex, Robot& robot, const Grid& grid, int& sequence);

    // 在控制器锁保护下构造快照（调用方须持有锁）。
    RobotSnapshot buildSnapshotLocked(const Robot& robot, int sequence) const;

    bool active_{false};
    std::vector<Direction> patrolPath_;
    std::size_t patrolIndex_{0};
    std::thread thread_;
    std::condition_variable cv_;
    std::atomic<std::chrono::milliseconds> intervalMs_{
        std::chrono::milliseconds(250)};
    std::string robotId_;
    std::string instanceId_;
    std::function<void(const RobotSnapshot&)> onStep_;
};

}  // namespace patrol
