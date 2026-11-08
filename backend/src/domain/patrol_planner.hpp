#pragma once

#include <vector>

#include "domain/grid.hpp"
#include "domain/types.hpp"

namespace patrol {

// 覆盖路径规划（组合策略）：同时计算 Boustrophedon 单元分解与贪心 BFS，
// 返回两者中较短的一条。保证路径长度 ≤ 贪心策略，且在多数含障碍地图上
// 更短；无障碍时两者均退化为全局蛇形扫描（N 格恰好 N-1 步，最优）。
// 返回空向量表示起点不可通行或没有可访问的未覆盖格（如被障碍包围）。
std::vector<Direction> planPatrolPath(const Grid& grid, Position start);

// 旧版贪心策略（保留用于对比测试）：每步 BFS 前往最近未访问格。
std::vector<Direction> planPatrolPathGreedy(const Grid& grid, Position start);

// Boustrophedon 单元分解：把可通行区域按障碍边界精确分解成矩形单元，
// 单元内蛇形扫描一次覆盖全部格子（单元内零冗余），单元之间用 BFS 最短
// 路径衔接，并按邻接顺序贪心排序、对每个单元做「入口+出口」一步前瞻选
// 蛇形变体。无障碍时退化为全局蛇形扫描。
std::vector<Direction> planPatrolPathBoustrophedon(const Grid& grid,
                                                   Position start);

}  // namespace patrol
