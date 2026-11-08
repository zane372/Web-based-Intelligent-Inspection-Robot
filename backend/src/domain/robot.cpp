#include "domain/robot.hpp"

namespace patrol {

Robot::Robot(Position initialPosition, Direction initialHeading)
    : position_(initialPosition),
      heading_(initialHeading),
      motionStatus_(MotionStatus::Idle),
      lastResult_(LastResult::None) {}

MoveResult Robot::applyManualMove(Direction direction, const Grid& grid) {
    // 规则 1：无论是否越界或受阻，朝向都更新为命令方向。
    heading_ = direction;

    // 规则 2：计算候选位置。
    const Position d = delta(direction);
    const Position candidate{position_.x + d.x, position_.y + d.y};

    // 规则 3：越界时位置不变，标记 Blocked。
    if (!grid.contains(candidate)) {
        motionStatus_ = MotionStatus::Blocked;
        lastResult_ = LastResult::BlockedBoundary;
        return MoveResult::BlockedBoundary;
    }

    // 规则 4：前方为障碍物，位置不变，标记 Blocked。
    if (grid.isObstacle(candidate)) {
        motionStatus_ = MotionStatus::Blocked;
        lastResult_ = LastResult::BlockedObstacle;
        return MoveResult::BlockedObstacle;
    }

    // 规则 5：可通行，移动成功并恢复 Idle。
    position_ = candidate;
    motionStatus_ = MotionStatus::Idle;
    lastResult_ = LastResult::Moved;
    return MoveResult::Moved;
}

void Robot::reset(Position initialPosition, Direction initialHeading) {
    position_ = initialPosition;
    heading_ = initialHeading;
    motionStatus_ = MotionStatus::Idle;
    lastResult_ = LastResult::Reset;
}

}  // namespace patrol
