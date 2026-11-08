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

// ---- 自动巡检 ----

// 3x3 无障碍小地图，确定性驱动巡检。
RobotController makeSmallController() {
    Grid grid(3, 3);
    Robot robot(Position{0, 0}, Direction::Right);
    // 调大间隔，避免后台线程干扰确定性驱动；经构造函数传入以保持
    // 返回值为纯右值（依赖 C++17 保证复制省略，避免拷贝构造）。
    return RobotController(grid, robot, "robot-1", "inst-patrol", 600000);
}

// 启动巡检：patrolActive 置位、结果 PatrolStarted、序列号 +1。
TEST(RobotControllerTest, StartPatrolActivates) {
    RobotController controller = makeSmallController();
    const CommandOutcome outcome =
        controller.execute(CommandType::StartPatrol, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::PatrolStarted);
    EXPECT_TRUE(outcome.robot.patrolActive);
    EXPECT_EQ(outcome.robot.lastResult, LastResult::PatrolStarted);
    EXPECT_EQ(outcome.robot.sequence, 1);
}

// 巡检推进一步：位置变化、序列号 +1、仍在巡检中。
TEST(RobotControllerTest, PatrolStepAdvances) {
    RobotController controller = makeSmallController();
    controller.execute(CommandType::StartPatrol, Direction::Right);

    EXPECT_TRUE(controller.patrolStep());
    RobotSnapshot snap = controller.snapshot();
    EXPECT_TRUE(snap.patrolActive);
    EXPECT_EQ(snap.sequence, 2);  // 启动 +1，推进一步 +1
}

// 巡检遍历完所有可通行格后自动完成：patrolActive 清除、结果 PatrolCompleted。
TEST(RobotControllerTest, PatrolCompletesAutomatically) {
    RobotController controller = makeSmallController();
    controller.execute(CommandType::StartPatrol, Direction::Right);

    // 3x3 无障碍共 9 格，起点已访问，需 8 步遍历完毕。
    for (int i = 0; i < 8; ++i) {
        ASSERT_TRUE(controller.patrolStep()) << "第 " << i << " 步仍应在巡检中";
    }
    const RobotSnapshot snap = controller.snapshot();
    EXPECT_FALSE(snap.patrolActive);
    EXPECT_EQ(snap.lastResult, LastResult::PatrolCompleted);
    EXPECT_EQ(snap.sequence, 1 + 8);  // 启动 1 + 8 步
}

// 停止巡检：patrolActive 清除、结果 PatrolStopped、序列号 +1。
TEST(RobotControllerTest, StopPatrolStops) {
    RobotController controller = makeSmallController();
    controller.execute(CommandType::StartPatrol, Direction::Right);
    controller.patrolStep();

    const CommandOutcome outcome =
        controller.execute(CommandType::StopPatrol, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::PatrolStopped);
    EXPECT_FALSE(outcome.robot.patrolActive);
    EXPECT_EQ(outcome.robot.lastResult, LastResult::PatrolStopped);
    EXPECT_EQ(outcome.robot.sequence, 3);  // 启动 + 一步 + 停止
}

// 手动命令隐式停止巡检。
TEST(RobotControllerTest, ManualMoveStopsPatrol) {
    RobotController controller = makeSmallController();
    controller.execute(CommandType::StartPatrol, Direction::Right);
    controller.patrolStep();

    const CommandOutcome outcome =
        controller.execute(CommandType::Move, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::Moved);
    EXPECT_FALSE(outcome.robot.patrolActive);
}

// 已在巡检时再次启动为幂等空操作：不改变序列号。
TEST(RobotControllerTest, StartPatrolWhileActiveIsIdempotent) {
    RobotController controller = makeSmallController();
    controller.execute(CommandType::StartPatrol, Direction::Right);
    const int seqBefore = controller.snapshot().sequence;

    const CommandOutcome outcome =
        controller.execute(CommandType::StartPatrol, Direction::Right);
    EXPECT_EQ(outcome.result, CommandResult::PatrolStarted);
    EXPECT_TRUE(outcome.robot.patrolActive);
    EXPECT_EQ(outcome.robot.sequence, seqBefore);
}

}  // namespace
}  // namespace patrol
