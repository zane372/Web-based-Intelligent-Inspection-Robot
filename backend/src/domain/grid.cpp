#include "domain/grid.hpp"

namespace patrol {

Grid::Grid(int width, int height, std::vector<Position> obstacles)
    : width_(width), height_(height) {
    for (const Position p : obstacles) {
        if (p.x >= 0 && p.x < width_ && p.y >= 0 && p.y < height_) {
            obstacles_.insert(p.y * width_ + p.x);
        }
    }
}

bool Grid::contains(Position position) const {
    return position.x >= 0 && position.x < width_ &&
           position.y >= 0 && position.y < height_;
}

bool Grid::isObstacle(Position position) const {
    // 界外坐标一律返回 false（不是障碍），避免越界坐标索引别名（如宽 10 时
    // {10, 0} 与 {0, 1} 键同为 10）被误判为障碍（FND-018）。
    if (!contains(position)) {
        return false;
    }
    return obstacles_.count(position.y * width_ + position.x) > 0;
}

bool Grid::isWalkable(Position position) const {
    return contains(position) && !isObstacle(position);
}

}  // namespace patrol
