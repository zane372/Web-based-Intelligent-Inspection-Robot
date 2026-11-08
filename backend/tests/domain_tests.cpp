// Domain 单元测试：覆盖四方向移动、边界阻止、恢复与四角边界。
// 只依赖 domain 模块，不启动 HTTP 服务，不依赖 JSON/Web。
#include <gtest/gtest.h>

#include "domain/grid.hpp"
#include "domain/robot.hpp"

namespace patrol {
namespace {

// 默认地图 20x20（宽=列数=20，高=行数=20）。
constexpr int kWidth = 20;
constexpr int kHeight = 20;

Grid defaultGrid() { return Grid(kWidth, kHeight); }

Robot robotAt(Position p, Direction heading) { return Robot(p, heading); }

// 断言一次移动后的完整状态。
void expectMove(Robot& robot, Direction command, Position expectedPos,
                Direction expectedHeading, MoveResult expectedResult,
                MotionStatus expectedMotion, LastResult expectedLast) {
    const MoveResult result = robot.applyManualMove(command, defaultGrid());
    EXPECT_EQ(result, expectedResult);
    EXPECT_EQ(robot.position(), expectedPos);
    EXPECT_EQ(robot.heading(), expectedHeading);
    EXPECT_EQ(robot.motionStatus(), expectedMotion);
    EXPECT_EQ(robot.lastResult(), expectedLast);
}

// SIM-01
TEST(DomainRobotTest, MoveUp) {
    Robot robot = robotAt({5, 5}, Direction::Right);
    expectMove(robot, Direction::Up, {5, 4}, Direction::Up,
               MoveResult::Moved, MotionStatus::Idle, LastResult::Moved);
}

// SIM-02
TEST(DomainRobotTest, MoveDown) {
    Robot robot = robotAt({5, 5}, Direction::Up);
    expectMove(robot, Direction::Down, {5, 6}, Direction::Down,
               MoveResult::Moved, MotionStatus::Idle, LastResult::Moved);
}

// SIM-03
TEST(DomainRobotTest, MoveLeft) {
    Robot robot = robotAt({5, 5}, Direction::Right);
    expectMove(robot, Direction::Left, {4, 5}, Direction::Left,
               MoveResult::Moved, MotionStatus::Idle, LastResult::Moved);
}

// SIM-04
TEST(DomainRobotTest, MoveRight) {
    Robot robot = robotAt({5, 5}, Direction::Left);
    expectMove(robot, Direction::Right, {6, 5}, Direction::Right,
               MoveResult::Moved, MotionStatus::Idle, LastResult::Moved);
}

// SIM-05：左边界向左，位置不变，朝向更新为 LEFT。
TEST(DomainRobotTest, BlockedAtLeftBoundary) {
    Robot robot = robotAt({0, 5}, Direction::Right);
    expectMove(robot, Direction::Left, {0, 5}, Direction::Left,
               MoveResult::BlockedBoundary, MotionStatus::Blocked,
               LastResult::BlockedBoundary);
}

// SIM-06：右边界向右。
TEST(DomainRobotTest, BlockedAtRightBoundary) {
    Robot robot = robotAt({19, 5}, Direction::Left);
    expectMove(robot, Direction::Right, {19, 5}, Direction::Right,
               MoveResult::BlockedBoundary, MotionStatus::Blocked,
               LastResult::BlockedBoundary);
}

// SIM-07：上边界向上。
TEST(DomainRobotTest, BlockedAtTopBoundary) {
    Robot robot = robotAt({5, 0}, Direction::Right);
    expectMove(robot, Direction::Up, {5, 0}, Direction::Up,
               MoveResult::BlockedBoundary, MotionStatus::Blocked,
               LastResult::BlockedBoundary);
}

// SIM-08：下边界向下。
TEST(DomainRobotTest, BlockedAtBottomBoundary) {
    Robot robot = robotAt({5, 19}, Direction::Down);
    expectMove(robot, Direction::Down, {5, 19}, Direction::Down,
               MoveResult::BlockedBoundary, MotionStatus::Blocked,
               LastResult::BlockedBoundary);
}

// SIM-09：边界阻止不是锁死状态，下一次界内移动可恢复。
TEST(DomainRobotTest, RecoverAfterBlocked) {
    Robot robot = robotAt({0, 0}, Direction::Right);
    // 先制造一次越界（向上），进入 Blocked。
    expectMove(robot, Direction::Up, {0, 0}, Direction::Up,
               MoveResult::BlockedBoundary, MotionStatus::Blocked,
               LastResult::BlockedBoundary);
    // 再向右，应恢复 Idle 并成功移动。
    expectMove(robot, Direction::Right, {1, 0}, Direction::Right,
               MoveResult::Moved, MotionStatus::Idle, LastResult::Moved);
}

// SIM-10（Domain 部分）：重置恢复给定初始姿态。
TEST(DomainRobotTest, ResetRestoresInitialPose) {
    Robot robot = robotAt({7, 8}, Direction::Down);
    robot.reset({0, 0}, Direction::Right);
    EXPECT_EQ(robot.position(), Position({0, 0}));
    EXPECT_EQ(robot.heading(), Direction::Right);
    EXPECT_EQ(robot.motionStatus(), MotionStatus::Idle);
    EXPECT_EQ(robot.lastResult(), LastResult::Reset);
}

// SIM-12：四个角点各尝试两个越界方向，位置始终在界内。
TEST(DomainRobotTest, FourCornersStayInBounds) {
    struct CornerCase {
        Position corner;
        Direction blocked1;
        Direction blocked2;
    };
    const CornerCase cases[] = {
        {{0, 0}, Direction::Up, Direction::Left},
        {{kWidth - 1, 0}, Direction::Up, Direction::Right},
        {{0, kHeight - 1}, Direction::Down, Direction::Left},
        {{kWidth - 1, kHeight - 1}, Direction::Down, Direction::Right},
    };

    for (const auto& c : cases) {
        Robot robot = robotAt(c.corner, Direction::Right);

        expectMove(robot, c.blocked1, c.corner, c.blocked1,
                   MoveResult::BlockedBoundary, MotionStatus::Blocked,
                   LastResult::BlockedBoundary);
        expectMove(robot, c.blocked2, c.corner, c.blocked2,
                   MoveResult::BlockedBoundary, MotionStatus::Blocked,
                   LastResult::BlockedBoundary);

        // 位置始终在矩形范围内。
        EXPECT_TRUE(defaultGrid().contains(robot.position()));
    }
}

// 初始状态：位置、朝向取自构造参数，运行状态 Idle、结果 None。
TEST(DomainRobotTest, InitialState) {
    Robot robot = robotAt({3, 4}, Direction::Left);
    EXPECT_EQ(robot.position(), Position({3, 4}));
    EXPECT_EQ(robot.heading(), Direction::Left);
    EXPECT_EQ(robot.motionStatus(), MotionStatus::Idle);
    EXPECT_EQ(robot.lastResult(), LastResult::None);
}

// 方向增量正确性。
TEST(DomainTypesTest, DeltaTable) {
    EXPECT_EQ(delta(Direction::Up), Position({0, -1}));
    EXPECT_EQ(delta(Direction::Down), Position({0, 1}));
    EXPECT_EQ(delta(Direction::Left), Position({-1, 0}));
    EXPECT_EQ(delta(Direction::Right), Position({1, 0}));
}

// Grid 边界判断。
TEST(DomainGridTest, Contains) {
    const Grid grid(20, 20);
    EXPECT_TRUE(grid.contains({0, 0}));
    EXPECT_TRUE(grid.contains({19, 19}));
    EXPECT_TRUE(grid.contains({10, 10}));
    EXPECT_FALSE(grid.contains({-1, 0}));
    EXPECT_FALSE(grid.contains({0, -1}));
    EXPECT_FALSE(grid.contains({20, 0}));
    EXPECT_FALSE(grid.contains({0, 20}));
}

}  // namespace
}  // namespace patrol
