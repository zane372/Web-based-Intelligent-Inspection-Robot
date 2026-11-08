#pragma once

#include <unordered_set>
#include <vector>

#include "domain/types.hpp"

namespace patrol {

// 矩形二维网格地图：判断坐标是否在矩形范围内，并标记障碍物单元格。
class Grid {
public:
    // width/height 为列数、行数，范围 [1, 100]，由配置加载层保证合法。
    // obstacles 为不可通行单元格（可选，默认无障碍）。
    explicit Grid(int width, int height,
                  std::vector<Position> obstacles = {});

    // 判断坐标是否在 [0, width) x [0, height) 范围内。
    bool contains(Position position) const;

    // 判断坐标是否为障碍物（仅对界内坐标有意义）。
    bool isObstacle(Position position) const;

    // 可通行 = 在界内且非障碍物。
    bool isWalkable(Position position) const;

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_;
    int height_;
    // 障碍物坐标编码为 y * width + x，O(1) 查询。
    std::unordered_set<int> obstacles_;
};

}  // namespace patrol
