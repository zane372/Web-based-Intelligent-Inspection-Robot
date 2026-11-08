#include "domain/grid.hpp"

namespace patrol {

Grid::Grid(int width, int height)
    : width_(width), height_(height) {}

bool Grid::contains(Position position) const {
    return position.x >= 0 && position.x < width_ &&
           position.y >= 0 && position.y < height_;
}

}  // namespace patrol
