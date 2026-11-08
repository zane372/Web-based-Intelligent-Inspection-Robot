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
    return obstacles_.count(position.y * width_ + position.x) > 0;
}

bool Grid::isWalkable(Position position) const {
    return contains(position) && !isObstacle(position);
}

}  // namespace patrol
