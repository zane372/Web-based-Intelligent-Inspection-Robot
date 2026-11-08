// 覆盖路径规划单元测试：覆盖性、确定性、避障与无出路场景。
#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "domain/grid.hpp"
#include "domain/patrol_planner.hpp"

namespace patrol {
namespace {

// 模拟执行路径，返回访问过的格子集合；同时断言每步相邻且可通行。
std::set<Position> simulate(const Grid& grid, Position start,
                            const std::vector<Direction>& path) {
    std::set<Position> visited{start};
    Position cur = start;
    for (const Direction d : path) {
        const Position step = delta(d);
        const Position next{cur.x + step.x, cur.y + step.y};
        EXPECT_TRUE(grid.isWalkable(next)) << "路径踩到不可通行格 (" << next.x
                                           << ',' << next.y << ')';
        cur = next;
        visited.insert(cur);
    }
    return visited;
}

// 收集所有可通行格。
std::set<Position> allWalkable(const Grid& grid) {
    std::set<Position> cells;
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            if (grid.isWalkable({x, y})) {
                cells.insert({x, y});
            }
        }
    }
    return cells;
}

// 空地图：覆盖全部格子，路径长度 = 格子数 - 1。
TEST(PatrolPlannerTest, CoversAllCellsOnEmptyGrid) {
    const Grid grid(5, 5);
    const std::vector<Direction> path = planPatrolPath(grid, {0, 0});
    EXPECT_EQ(path.size(), 24u);
    EXPECT_EQ(simulate(grid, {0, 0}, path), allWalkable(grid));
}

// 障碍物地图：覆盖所有可通行格且避开障碍物。
TEST(PatrolPlannerTest, CoversAllCellsWithObstacles) {
    // 中间竖墙 (2,0)-(2,2)，底部 (2,3)/(2,4) 开放，形成绕行与回头。
    const Grid grid(5, 5, {{2, 0}, {2, 1}, {2, 2}});
    const std::vector<Direction> path = planPatrolPath(grid, {0, 0});
    EXPECT_EQ(simulate(grid, {0, 0}, path), allWalkable(grid));
}

// 复杂障碍布局（含死胡同）仍覆盖全部可通行格。
TEST(PatrolPlannerTest, CoversAllCellsWithScatteredObstacles) {
    const Grid grid(6, 6, {{1, 1}, {2, 1}, {4, 2}, {4, 3}, {4, 4}, {0, 5}});
    const std::vector<Direction> path = planPatrolPath(grid, {0, 0});
    EXPECT_EQ(simulate(grid, {0, 0}, path), allWalkable(grid));
}

// 确定性：同一地图与起点，两次结果一致。
TEST(PatrolPlannerTest, Deterministic) {
    const Grid grid(6, 6, {{2, 2}, {3, 2}, {2, 3}});
    const std::vector<Direction> a = planPatrolPath(grid, {0, 0});
    const std::vector<Direction> b = planPatrolPath(grid, {0, 0});
    EXPECT_EQ(a, b);
}

// 起点被障碍包围：无可访问格，返回空路径。
TEST(PatrolPlannerTest, SurroundedStartReturnsEmpty) {
    // (0,0) 左/上越界，右 (1,0) 与下 (0,1) 为障碍 → 无路可走。
    const Grid grid(3, 3, {{1, 0}, {0, 1}});
    EXPECT_TRUE(planPatrolPath(grid, {0, 0}).empty());
}

// 起点本身在障碍上：返回空路径。
TEST(PatrolPlannerTest, StartOnObstacleReturnsEmpty) {
    const Grid grid(3, 3, {{0, 0}});
    EXPECT_TRUE(planPatrolPath(grid, {0, 0}).empty());
}

}  // namespace
}  // namespace patrol
