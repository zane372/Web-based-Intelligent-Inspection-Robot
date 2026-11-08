#include "domain/patrol_planner.hpp"

#include <queue>

namespace patrol {

namespace {

// 四方向（顺序固定，保证结果确定）：上、下、左、右。
constexpr Direction kDirections[] = {
    Direction::Up, Direction::Down, Direction::Left, Direction::Right,
};

// 从 current 出发做 BFS，返回「通往最近未访问可通行格的第一步方向」；
// 若无可达的未访问格则返回 false。visited 已把障碍物以外的已访问格标记为 true。
bool findFirstStepToNearestUnvisited(const Grid& grid,
                                     const std::vector<std::vector<bool>>& visited,
                                     Position current,
                                     Direction& out) {
    const int width = grid.width();
    const int height = grid.height();

    // dist：-1 表示未进入队列；firstStep：到达该格的「当前步方向」。
    std::vector<std::vector<int>> dist(height, std::vector<int>(width, -1));
    std::vector<std::vector<Direction>> firstStep(
        height, std::vector<Direction>(width, Direction::Right));

    std::queue<Position> queue;
    dist[current.y][current.x] = 0;
    queue.push(current);

    while (!queue.empty()) {
        const Position p = queue.front();
        queue.pop();

        // BFS 出队顺序即距离递增；第一个「未访问」格就是最近目标。
        if (p != current && !visited[p.y][p.x]) {
            out = firstStep[p.y][p.x];
            return true;
        }

        for (const Direction dir : kDirections) {
            const Position d = delta(dir);
            const Position np{p.x + d.x, p.y + d.y};
            if (!grid.isWalkable(np) || dist[np.y][np.x] != -1) {
                continue;
            }
            dist[np.y][np.x] = dist[p.y][p.x] + 1;
            firstStep[np.y][np.x] = (p == current) ? dir : firstStep[p.y][p.x];
            queue.push(np);
        }
    }
    return false;
}

}  // namespace

std::vector<Direction> planPatrolPath(const Grid& grid, Position start) {
    std::vector<Direction> path;

    // 起点必须可通行（由配置层保证不在障碍上），否则无路径可规划。
    if (!grid.isWalkable(start)) {
        return path;
    }

    const int width = grid.width();
    const int height = grid.height();
    std::vector<std::vector<bool>> visited(height,
                                           std::vector<bool>(width, false));
    visited[start.y][start.x] = true;

    Position current = start;
    Direction firstDir{};
    while (findFirstStepToNearestUnvisited(grid, visited, current, firstDir)) {
        const Position d = delta(firstDir);
        current = Position{current.x + d.x, current.y + d.y};
        visited[current.y][current.x] = true;
        path.push_back(firstDir);
    }
    return path;
}

}  // namespace patrol
