// Controller 单元测试：序列号单调递增、快照一致性、Reset 序列号语义。
#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "control/robot_controller.hpp"

namespace patrol {
namespace {

RobotController makeController() {
    Grid grid(20, 20);
    Robot robot(Position{0, 0}, Direction::Right);
    return RobotController(grid, robot, "robot-1", "inst-test");
}

TEST(RobotControllerTest, InitialSnapshotSequenceZero) {
    RobotController controller = makeController();
    const RobotSnapshot snap = controller.snapshot();
    EXPECT_EQ(snap.robotId, "robot-1");
    EXPECT_EQ(snap.instanceId, "inst-test");
    EXPECT_EQ(snap.position, Position({0, 0}));
    EXPECT_EQ(snap.heading, Direction::Right);
    EXPECT_EQ(snap.motionStatus, MotionStatus::Idle);
    EXPECT_EQ(snap.lastResult, LastResult::None);
    EXPECT_EQ(snap.sequence, 0);
}

// SIM-10：成功移动序列号 +1。
TEST(RobotControllerTest, MoveIncrementsSequence) {
    RobotController controller = makeController();
    CommandOutcome outcome = controller.execute(CommandType::Move, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::Moved);
    EXPECT_EQ(outcome.robot.sequence, 1);
    EXPECT_EQ(outcome.robot.position, Position({1, 0}));
    EXPECT_EQ(outcome.robot.lastResult, LastResult::Moved);
}

// SIM-10：越界阻止同样序列号 +1。
TEST(RobotControllerTest, BlockedIncrementsSequence) {
    RobotController controller = makeController();
    CommandOutcome outcome = controller.execute(CommandType::Move, Direction::Up);
    EXPECT_EQ(outcome.result, CommandResult::BlockedBoundary);
    EXPECT_EQ(outcome.robot.sequence, 1);
    EXPECT_EQ(outcome.robot.position, Position({0, 0}));
    EXPECT_EQ(outcome.robot.heading, Direction::Up);
    EXPECT_EQ(outcome.robot.motionStatus, MotionStatus::Blocked);
}

// SIM-10：Reset 恢复初始位姿并序列号 +1。
TEST(RobotControllerTest, ResetIncrementsSequenceAndRestoresPose) {
    RobotController controller = makeController();
    controller.execute(CommandType::Move, Direction::Right);
    controller.execute(CommandType::Move, Direction::Right);

    CommandOutcome outcome = controller.execute(CommandType::Reset, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::Reset);
    EXPECT_EQ(outcome.robot.sequence, 3);
    EXPECT_EQ(outcome.robot.position, Position({0, 0}));
    EXPECT_EQ(outcome.robot.heading, Direction::Right);
    EXPECT_EQ(outcome.robot.motionStatus, MotionStatus::Idle);
    EXPECT_EQ(outcome.robot.lastResult, LastResult::Reset);
}

// 只读快照不改变序列号。
TEST(RobotControllerTest, SnapshotDoesNotChangeSequence) {
    RobotController controller = makeController();
    controller.snapshot();
    controller.snapshot();
    EXPECT_EQ(controller.snapshot().sequence, 0);
}

// 多线程并发执行不丢序列号（总数 = 线程数）。
TEST(RobotControllerTest, ConcurrentExecuteKeepsSequenceAtomic) {
    RobotController controller = makeController();
    constexpr int kThreads = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&controller]() {
            controller.execute(CommandType::Move, Direction::Right);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    // 从 (0,0) 向右移动 50 次，x 停在右边界 19，序列号必须恰好等于 50。
    const RobotSnapshot snap = controller.snapshot();
    EXPECT_EQ(snap.sequence, kThreads);
    EXPECT_EQ(snap.position, Position({19, 0}));
}

}  // namespace
}  // namespace patrol
