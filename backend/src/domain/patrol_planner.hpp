#pragma once

#include <vector>

#include "domain/grid.hpp"
#include "domain/types.hpp"

namespace patrol {

// 覆盖路径规划：给定地图与起点，返回一串移动方向，
// 使机器人按「贪心 BFS 前往最近未访问格」的策略覆盖所有可通行格。
// 该策略对任意障碍布局都能绕行，且在无障碍时退化为蛇形扫描。
// 返回空向量表示起点没有可访问的未访问格（如被障碍包围）。
std::vector<Direction> planPatrolPath(const Grid& grid, Position start);

}  // namespace patrol
