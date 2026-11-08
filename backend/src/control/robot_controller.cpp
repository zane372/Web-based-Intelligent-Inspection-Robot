#include "control/robot_controller.hpp"

#include <utility>

namespace patrol {

RobotController::RobotController(Grid grid, Robot robot,
                                 std::string robotId, std::string instanceId)
    : grid_(grid),
      robot_(robot),
      initialPosition_(robot.position()),
      initialHeading_(robot.heading()),
      robotId_(std::move(robotId)),
      instanceId_(std::move(instanceId)) {}

RobotSnapshot RobotController::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buildSnapshotLocked();
}

CommandOutcome RobotController::execute(CommandType type, Direction direction) {
    std::lock_guard<std::mutex> lock(mutex_);

    CommandResult result = CommandResult::Reset;
    if (type == CommandType::Move) {
        const MoveResult move = robot_.applyManualMove(direction, grid_);
        result = (move == MoveResult::Moved)
                     ? CommandResult::Moved
                     : CommandResult::BlockedBoundary;
    } else {
        robot_.reset(initialPosition_, initialHeading_);
        result = CommandResult::Reset;
    }

    // 成功与阻止（含非法？）都占用一个序列号。
    // 注：非法请求在 API 层被拦截，不会到达这里，因此不占序列号。
    ++sequence_;

    return CommandOutcome{result, buildSnapshotLocked()};
}

RobotSnapshot RobotController::buildSnapshotLocked() const {
    return RobotSnapshot{
        robotId_,
        instanceId_,
        robot_.position(),
        robot_.heading(),
        robot_.motionStatus(),
        robot_.lastResult(),
        sequence_,
    };
}

}  // namespace patrol
