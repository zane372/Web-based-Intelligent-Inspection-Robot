#include "control/patrol_executor.hpp"

#include <utility>

#include "domain/patrol_planner.hpp"

namespace patrol {

void PatrolExecutor::start(std::vector<Direction> path, std::mutex& mutex,
                           Robot& robot, const Grid& grid, int& sequence) {
    // 递增代际并传给线程：loop 以该值为本次会话身份，重新加锁后若
    // generation_ 已变化（并发 Stop/Start）则立即退出，避免旧线程复活。
    ++generation_;
    const std::uint64_t generation = generation_;
    patrolPath_ = std::move(path);
    patrolIndex_ = 0;
    active_ = true;
    thread_ = std::thread(&PatrolExecutor::loop, this, std::ref(mutex),
                          std::ref(robot), std::ref(grid), std::ref(sequence),
                          generation);
}

std::thread PatrolExecutor::stopAndTakeThread() {
    // 递增代际使任何尚未退出的旧线程在下一次加锁后识别出代际失配而退出。
    ++generation_;
    active_ = false;
    cv_.notify_all();
    std::thread t;
    if (thread_.joinable()) {
        t = std::move(thread_);
    }
    return t;
}

void PatrolExecutor::setIdentity(std::string robotId, std::string instanceId) {
    robotId_ = std::move(robotId);
    instanceId_ = std::move(instanceId);
}

void PatrolExecutor::setOnStep(
    std::function<void(const RobotSnapshot&)> callback) {
    onStep_ = std::move(callback);
}

RobotSnapshot PatrolExecutor::buildSnapshotLocked(const Robot& robot,
                                                  int sequence) const {
    RobotSnapshot s;
    s.robotId = robotId_;
    s.instanceId = instanceId_;
    s.position = robot.position();
    s.heading = robot.heading();
    s.motionStatus = robot.motionStatus();
    s.lastResult = robot.lastResult();
    s.sequence = sequence;
    s.patrolActive = active_;
    return s;
}

bool PatrolExecutor::stepReplanned(std::unique_lock<std::mutex>& lock,
                                   Robot& robot, const Grid& grid,
                                   int& sequence) {
    if (!active_) {
        stepSnapshot_ = buildSnapshotLocked(robot, sequence);
        return false;
    }
    if (patrolIndex_ >= patrolPath_.size()) {
        active_ = false;
        cv_.notify_all();
        robot.setMotionStatus(MotionStatus::Idle);
        robot.setLastResult(LastResult::PatrolCompleted);
        stepSnapshot_ = buildSnapshotLocked(robot, sequence);
        return false;
    }
    const Direction dir = patrolPath_[patrolIndex_];
    // 防御性障碍/边界检测：规划路径本应避开障碍，若异常命中（如陈旧路径）
    // 则报警 BLOCKED_OBSTACLE/BLOCKED_BOUNDARY，并从当前位置重规划剩余路径。
    const Position d = delta(dir);
    const Position candidate{robot.position().x + d.x, robot.position().y + d.y};
    if (!grid.contains(candidate) || grid.isObstacle(candidate)) {
        // 复刻 applyManualMove 的「先更新朝向」规则：受阻也更新朝向。
        robot.setHeading(dir);
        robot.setMotionStatus(MotionStatus::Blocked);
        robot.setLastResult(grid.contains(candidate)
                                ? LastResult::BlockedObstacle
                                : LastResult::BlockedBoundary);
        ++sequence;
        // 受阻快照在解锁前捕获：确保该序列的观察（CONTACT 事件）不被
        // 并发命令「偷走」序列号（审查 I2）。
        stepSnapshot_ = buildSnapshotLocked(robot, sequence);
        // 全图规划为 O(N²) 且伴随堆分配，移出临界区执行（AR-CON-002/003）。
        const Position from = robot.position();
        lock.unlock();
        std::vector<Direction> path = planPatrolPath(grid, from);
        lock.lock();
        if (!active_) {
            return false;  // 重规划期间巡检被停止。
        }
        if (!(robot.position() == from)) {
            // 位置被并发移动（罕见）：就地重规划。
            path = planPatrolPath(grid, robot.position());
        }
        patrolPath_ = std::move(path);
        patrolIndex_ = 0;
        if (patrolPath_.empty()) {
            // 无可遍历路径：视为巡检完成。保持受阻快照（CONTACT 事件），
            // 完成态由 /api/v1/robot 的 lastResult=PATROL_COMPLETED 反映。
            active_ = false;
            cv_.notify_all();
            robot.setMotionStatus(MotionStatus::Idle);
            robot.setLastResult(LastResult::PatrolCompleted);
            return false;
        }
        return true;
    }
    ++patrolIndex_;
    robot.applyManualMove(dir, grid);
    ++sequence;
    const bool completed = patrolIndex_ >= patrolPath_.size();
    if (completed) {
        // 最后一步执行完毕：标记完成并返回 false，使 loop 在锁外通知观察者后
        // 立即 break 退出。若此处返回 true，loop 会重新加锁并再次进入
        // while(active_)，形成「active_ 已置 false 但旧线程尚未退出」的窗口，
        // 窗口内并发 StartPatrol 会把 active_ 重新置 true，导致旧线程与新线程
        // 双线程同时推进同一巡检（sequence 翻倍、2 倍速、StartPatrol 响应被
        // join 阻塞）。返回 false 后，loop 仍会先 onStep_(最终快照) 再 break，
        // 最终步的观察通知不丢失。
        active_ = false;
        cv_.notify_all();
        robot.setMotionStatus(MotionStatus::Idle);
        robot.setLastResult(LastResult::PatrolCompleted);
    }
    stepSnapshot_ = buildSnapshotLocked(robot, sequence);
    return !completed;
}

void PatrolExecutor::loop(std::mutex& mutex, Robot& robot, const Grid& grid,
                          int& sequence, std::uint64_t generation) {
    std::unique_lock<std::mutex> lock(mutex);
    while (active_ && generation_ == generation) {
        cv_.wait_for(lock, intervalMs_.load(), [this, generation] {
            return !active_ || generation_ != generation;
        });
        if (!active_ || generation_ != generation) {
            break;
        }
        // stepReplanned 以锁被持有的状态进入，内部仅在规划时短暂释放锁。
        const bool keepGoing = stepReplanned(lock, robot, grid, sequence);
        // stepReplanned 已在锁内捕获已确认快照；于锁外通知（AR-CON-006/007）。
        const RobotSnapshot snap = stepSnapshot_;
        lock.unlock();
        if (onStep_) {
            onStep_(snap);
        }
        if (!keepGoing) {
            break;  // 锁已释放，线程退出。
        }
        lock.lock();
    }
}

}  // namespace patrol
