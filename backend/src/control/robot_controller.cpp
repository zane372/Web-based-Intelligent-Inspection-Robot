#include "control/robot_controller.hpp"

#include <utility>

#include "domain/patrol_planner.hpp"

namespace patrol {

namespace {

CommandResult mapMoveResult(MoveResult mr) {
    switch (mr) {
        case MoveResult::Moved:
            return CommandResult::Moved;
        case MoveResult::BlockedBoundary:
            return CommandResult::BlockedBoundary;
        case MoveResult::BlockedObstacle:
            return CommandResult::BlockedObstacle;
    }
    return CommandResult::Moved;  // 不可达，安抚编译器。
}

}  // namespace

RobotController::RobotController(Grid grid, Robot robot,
                                 std::string robotId, std::string instanceId,
                                 int patrolIntervalMs)
    : grid_(std::move(grid)),
      robot_(std::move(robot)),
      initialPosition_(robot_.position()),
      initialHeading_(robot_.heading()),
      robotId_(std::move(robotId)),
      instanceId_(std::move(instanceId)),
      patrolIntervalMs_(std::chrono::milliseconds(patrolIntervalMs)) {}

RobotController::~RobotController() {
    stopPatrolAndJoin();
}

RobotSnapshot RobotController::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buildSnapshotLocked();
}

CommandOutcome RobotController::execute(CommandType type, Direction direction) {
    std::thread threadToJoin;
    CommandResult result{};
    RobotSnapshot snap;

    {
        std::unique_lock<std::mutex> lock(mutex_);

        switch (type) {
            case CommandType::Move:
            case CommandType::Reset: {
                // 隐式停止进行中的巡检（若有），并回收任何残留线程。
                threadToJoin = takePatrolThreadLocked();
                if (type == CommandType::Move) {
                    result =
                        mapMoveResult(robot_.applyManualMove(direction, grid_));
                } else {
                    robot_.reset(initialPosition_, initialHeading_);
                    result = CommandResult::Reset;
                }
                ++sequence_;
                break;
            }
            case CommandType::StartPatrol: {
                if (patrolActive_) {
                    // 已在巡检：幂等空操作，不改变状态与序列号。
                    result = CommandResult::PatrolStarted;
                } else {
                    // 回收残留线程（若有：上一轮巡检刚结束尚未 join）。
                    threadToJoin = takePatrolThreadLocked();
                    // I-1 修复：路径规划为 O(N²) 且伴随大量堆分配，若在锁内执行
                    // 会阻塞全部 API 路由（最大 100×100 地图下可达数秒）。
                    // grid_ 运行时只读、规划仅依赖当前位置，故移出临界区执行。
                    const Position plannedFrom = robot_.position();
                    lock.unlock();
                    std::vector<Direction> path =
                        planPatrolPath(grid_, plannedFrom);
                    lock.lock();
                    if (patrolActive_) {
                        // 解锁期间已有并发命令启动巡检：丢弃本次规划，幂等返回。
                        result = CommandResult::PatrolStarted;
                    } else {
                        // 位置若被并发移动则就地重规划（罕见路径）。
                        if (!(robot_.position() == plannedFrom)) {
                            path = planPatrolPath(grid_, robot_.position());
                        }
                        patrolPath_ = std::move(path);
                        if (patrolPath_.empty()) {
                            robot_.setMotionStatus(MotionStatus::Idle);
                            robot_.setLastResult(LastResult::PatrolCompleted);
                            // 无可遍历路径：命令仍受理（PATROL_STARTED），
                            // 巡检状态由 lastResult=PATROL_COMPLETED 体现立即完成。
                            result = CommandResult::PatrolStarted;
                            ++sequence_;
                        } else {
                            patrolIndex_ = 0;
                            patrolActive_ = true;
                            robot_.setMotionStatus(MotionStatus::Idle);
                            robot_.setLastResult(LastResult::PatrolStarted);
                            result = CommandResult::PatrolStarted;
                            ++sequence_;
                            patrolThread_ =
                                std::thread(&RobotController::patrolLoop, this);
                        }
                    }
                }
                break;
            }
            case CommandType::StopPatrol: {
                if (patrolActive_) {
                    threadToJoin = takePatrolThreadLocked();
                    robot_.setMotionStatus(MotionStatus::Idle);
                    robot_.setLastResult(LastResult::PatrolStopped);
                    result = CommandResult::PatrolStopped;
                    ++sequence_;
                } else {
                    // 未在巡检：回收残留线程（若有），幂等空操作。
                    threadToJoin = takePatrolThreadLocked();
                    result = CommandResult::PatrolStopped;
                }
                break;
            }
        }
        snap = buildSnapshotLocked();
    }

    if (threadToJoin.joinable()) {
        threadToJoin.join();
    }
    return {result, snap};
}

bool RobotController::patrolStep() {
    std::lock_guard<std::mutex> lock(mutex_);
    return patrolStepLocked();
}

RobotSnapshot RobotController::buildSnapshotLocked() const {
    RobotSnapshot s;
    s.robotId = robotId_;
    s.instanceId = instanceId_;
    s.position = robot_.position();
    s.heading = robot_.heading();
    s.motionStatus = robot_.motionStatus();
    s.lastResult = robot_.lastResult();
    s.sequence = sequence_;
    s.patrolActive = patrolActive_;
    return s;
}

bool RobotController::patrolStepLocked() {
    if (!patrolActive_) {
        return false;
    }
    if (patrolIndex_ >= patrolPath_.size()) {
        patrolActive_ = false;
        patrolCv_.notify_all();
        robot_.setMotionStatus(MotionStatus::Idle);
        robot_.setLastResult(LastResult::PatrolCompleted);
        return false;
    }
    const Direction dir = patrolPath_[patrolIndex_++];
    robot_.applyManualMove(dir, grid_);
    ++sequence_;
    if (patrolIndex_ >= patrolPath_.size()) {
        // 最后一步执行完毕：标记完成，但仍返回 true 表示本步已推进。
        patrolActive_ = false;
        patrolCv_.notify_all();
        robot_.setMotionStatus(MotionStatus::Idle);
        robot_.setLastResult(LastResult::PatrolCompleted);
    }
    return true;
}

void RobotController::patrolLoop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (patrolActive_) {
        patrolCv_.wait_for(lock, patrolIntervalMs_.load(),
                           [this] { return !patrolActive_; });
        if (!patrolActive_) {
            break;
        }
        if (!patrolStepLocked()) {
            break;
        }
    }
}

std::thread RobotController::takePatrolThreadLocked() {
    patrolActive_ = false;
    patrolCv_.notify_all();
    std::thread t;
    if (patrolThread_.joinable()) {
        t = std::move(patrolThread_);
    }
    return t;
}

void RobotController::stopPatrolAndJoin() {
    std::thread t;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        t = takePatrolThreadLocked();
    }
    if (t.joinable()) {
        t.join();
    }
}

}  // namespace patrol
