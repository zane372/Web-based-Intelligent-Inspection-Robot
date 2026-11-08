#pragma once

#include "domain/types.hpp"

namespace patrol {

// 矩形二维网格地图。本阶段只负责判断坐标是否在矩形范围内，
// 不包含障碍物、巡检点或传感器区域。
class Grid {
public:
    // width/height 为列数、行数，范围 [1, 100]，由配置加载层保证合法。
    Grid(int width, int height);

    // 判断坐标是否在 [0, width) x [0, height) 范围内。
    bool contains(Position position) const;

    int width() const { return width_; }
    int height() const { return height_; }

private:
    int width_;
    int height_;
};

}  // namespace patrol
