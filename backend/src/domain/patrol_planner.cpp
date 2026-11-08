#include "domain/patrol_planner.hpp"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <vector>

namespace patrol {

namespace {

// 四方向（顺序固定，保证结果确定）：上、下、左、右。
constexpr Direction kDirections[] = {
    Direction::Up, Direction::Down, Direction::Left, Direction::Right,
};

// 一个「单元」：由障碍边界划分出的矩形可通行区域（列 [xLo,xHi) × 行 [yLo,yHi]）。
struct Cell {
    int xLo;
    int xHi;
    int yLo;
    int yHi;
};

int manhattan(Position a, Position b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// BFS 求 from→to 的最短方向序列（只要求目标可通行，可穿过任意可通行格）。
// 不可达时返回空向量。
std::vector<Direction> bfsPath(const Grid& grid, Position from, Position to) {
    const int width = grid.width();
    const int height = grid.height();
    std::vector<std::vector<int>> dist(height, std::vector<int>(width, -1));
    std::vector<std::vector<Position>> parent(
        height, std::vector<Position>(width, Position{-1, -1}));

    std::queue<Position> queue;
    dist[from.y][from.x] = 0;
    queue.push(from);

    while (!queue.empty()) {
        const Position p = queue.front();
        queue.pop();
        if (p == to) {
            std::vector<Direction> dirs;
            Position cur = to;
            while (!(cur == from)) {
                const Position par = parent[cur.y][cur.x];
                const int dx = cur.x - par.x;
                const int dy = cur.y - par.y;
                if (dx == 1) {
                    dirs.push_back(Direction::Right);
                } else if (dx == -1) {
                    dirs.push_back(Direction::Left);
                } else if (dy == 1) {
                    dirs.push_back(Direction::Down);
                } else {
                    dirs.push_back(Direction::Up);
                }
                cur = par;
            }
            std::reverse(dirs.begin(), dirs.end());
            return dirs;
        }
        for (const Direction dir : kDirections) {
            const Position d = delta(dir);
            const Position np{p.x + d.x, p.y + d.y};
            if (!grid.isWalkable(np) || dist[np.y][np.x] != -1) {
                continue;
            }
            dist[np.y][np.x] = dist[p.y][p.x] + 1;
            parent[np.y][np.x] = p;
            queue.push(np);
        }
    }
    return {};
}

// 精确单元分解（XOR 关键列法）：一列 x（1 ≤ x ≤ W）是「关键列」，当且仅当
// 存在某行 y 使得 (x-1,y) 与 (x,y) 的障碍状态不同（障碍左/右边缘）。关键列
// 之间构成竖直切片，片内可通行结构一致，再按连续行段切成矩形单元。
std::vector<Cell> decomposeCells(const Grid& grid) {
    const int width = grid.width();
    const int height = grid.height();

    std::vector<int> critical;
    critical.push_back(0);
    critical.push_back(width);
    for (int x = 1; x <= width; ++x) {
        for (int y = 0; y < height; ++y) {
            const bool leftObstacle = grid.isObstacle(Position{x - 1, y});
            const bool rightObstacle =
                (x < width) && grid.isObstacle(Position{x, y});
            if (leftObstacle != rightObstacle) {
                critical.push_back(x);
                break;
            }
        }
    }
    std::sort(critical.begin(), critical.end());
    critical.erase(std::unique(critical.begin(), critical.end()), critical.end());

    std::vector<Cell> cells;
    for (std::size_t i = 0; i + 1 < critical.size(); ++i) {
        const int xLo = critical[i];
        const int xHi = critical[i + 1];
        if (xLo >= xHi) {
            continue;
        }
        int y = 0;
        while (y < height) {
            if (!grid.isWalkable(Position{xLo, y})) {
                ++y;
                continue;
            }
            int yRun = y;
            while (yRun < height && grid.isWalkable(Position{xLo, yRun})) {
                ++yRun;
            }
            cells.push_back(Cell{xLo, xHi, y, yRun - 1});
            y = yRun;
        }
    }
    return cells;
}

// 竖直蛇形遍历：首列方向由 downFirst 决定，相邻列交替反向。
std::vector<Position> serpentineVertical(const Cell& c, bool downFirst) {
    std::vector<Position> order;
    order.reserve(static_cast<std::size_t>(c.xHi - c.xLo) *
                  static_cast<std::size_t>(c.yHi - c.yLo + 1));
    for (int x = c.xLo; x < c.xHi; ++x) {
        const bool goDown = ((x - c.xLo) % 2 == 0) ? downFirst : !downFirst;
        if (goDown) {
            for (int y = c.yLo; y <= c.yHi; ++y) {
                order.push_back(Position{x, y});
            }
        } else {
            for (int y = c.yHi; y >= c.yLo; --y) {
                order.push_back(Position{x, y});
            }
        }
    }
    return order;
}

// 水平蛇形遍历：首行方向由 rightFirst 决定，相邻行交替反向。
std::vector<Position> serpentineHorizontal(const Cell& c, bool rightFirst) {
    std::vector<Position> order;
    order.reserve(static_cast<std::size_t>(c.xHi - c.xLo) *
                  static_cast<std::size_t>(c.yHi - c.yLo + 1));
    for (int y = c.yLo; y <= c.yHi; ++y) {
        const bool goRight = ((y - c.yLo) % 2 == 0) ? rightFirst : !rightFirst;
        if (goRight) {
            for (int x = c.xLo; x < c.xHi; ++x) {
                order.push_back(Position{x, y});
            }
        } else {
            for (int x = c.xHi - 1; x >= c.xLo; --x) {
                order.push_back(Position{x, y});
            }
        }
    }
    return order;
}

// 从 start 出发的 BFS 可达性（只走可通行格）。
std::vector<std::vector<bool>> computeReachable(const Grid& grid, Position start) {
    const int width = grid.width();
    const int height = grid.height();
    std::vector<std::vector<bool>> reachable(
        height, std::vector<bool>(width, false));
    std::queue<Position> queue;
    reachable[start.y][start.x] = true;
    queue.push(start);
    while (!queue.empty()) {
        const Position p = queue.front();
        queue.pop();
        for (const Direction dir : kDirections) {
            const Position d = delta(dir);
            const Position np{p.x + d.x, p.y + d.y};
            if (!grid.isWalkable(np) || reachable[np.y][np.x]) {
                continue;
            }
            reachable[np.y][np.x] = true;
            queue.push(np);
        }
    }
    return reachable;
}

// 两个单元是否水平相邻（共享竖直边界且边界两侧某行都可通行）。
bool cellsAdjacent(const Grid& grid, const Cell& a, const Cell& b) {
    if (a.xHi == b.xLo) {
        const int yLo = std::max(a.yLo, b.yLo);
        const int yHi = std::min(a.yHi, b.yHi);
        if (yLo > yHi) {
            return false;
        }
        for (int y = yLo; y <= yHi; ++y) {
            if (grid.isWalkable(Position{a.xHi - 1, y}) &&
                grid.isWalkable(Position{b.xLo, y})) {
                return true;
            }
        }
        return false;
    }
    if (b.xHi == a.xLo) {
        return cellsAdjacent(grid, b, a);
    }
    return false;
}

// 单元内是否存在「可达且未覆盖」格。
bool cellHasUncovered(const Cell& cell,
                      const std::vector<std::vector<bool>>& reachable,
                      const std::vector<std::vector<bool>>& covered) {
    for (int y = cell.yLo; y <= cell.yHi; ++y) {
        for (int x = cell.xLo; x < cell.xHi; ++x) {
            if (reachable[y][x] && !covered[y][x]) {
                return true;
            }
        }
    }
    return false;
}

// 单元内「离 from 最近的可达且未覆盖格」（用于单元排序与入口评估）。
// 找不到时返回 false。
bool nearestUncoveredInCell(const Cell& cell, Position from,
                            const std::vector<std::vector<bool>>& reachable,
                            const std::vector<std::vector<bool>>& covered,
                            Position& out) {
    int bestDist = 1 << 30;
    bool found = false;
    for (int y = cell.yLo; y <= cell.yHi; ++y) {
        for (int x = cell.xLo; x < cell.xHi; ++x) {
            if (!reachable[y][x] || covered[y][x]) {
                continue;
            }
            const Position p{x, y};
            const int d = manhattan(from, p);
            if (d < bestDist) {
                bestDist = d;
                out = p;
                found = true;
            }
        }
    }
    return found;
}

// 从 current 出发做 BFS，返回「通往最近未访问可通行格的第一步方向」；
// 若无可达的未访问格则返回 false。（旧贪心策略用）
bool findFirstStepToNearestUnvisited(const Grid& grid,
                                     const std::vector<std::vector<bool>>& visited,
                                     Position current,
                                     Direction& out) {
    const int width = grid.width();
    const int height = grid.height();

    std::vector<std::vector<int>> dist(height, std::vector<int>(width, -1));
    std::vector<std::vector<Direction>> firstStep(
        height, std::vector<Direction>(width, Direction::Right));

    std::queue<Position> queue;
    dist[current.y][current.x] = 0;
    queue.push(current);

    while (!queue.empty()) {
        const Position p = queue.front();
        queue.pop();

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

std::vector<Direction> planPatrolPathBoustrophedon(const Grid& grid,
                                                   Position start) {
    std::vector<Direction> path;
    if (!grid.isWalkable(start)) {
        return path;
    }
    const int width = grid.width();
    const int height = grid.height();

    const std::vector<std::vector<bool>> reachable = computeReachable(grid, start);
    const std::vector<Cell> cells = decomposeCells(grid);

    // 找到起点所在单元。
    int startCell = -1;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (cells[i].xLo <= start.x && start.x < cells[i].xHi &&
            cells[i].yLo <= start.y && start.y <= cells[i].yHi) {
            startCell = static_cast<int>(i);
            break;
        }
    }
    if (startCell < 0) {
        return path;
    }

    // 单元邻接表。
    std::vector<std::vector<int>> adj(cells.size());
    for (std::size_t i = 0; i < cells.size(); ++i) {
        for (std::size_t j = i + 1; j < cells.size(); ++j) {
            if (cellsAdjacent(grid, cells[i], cells[j])) {
                adj[i].push_back(static_cast<int>(j));
                adj[j].push_back(static_cast<int>(i));
            }
        }
    }

    std::vector<std::vector<bool>> covered(height, std::vector<bool>(width, false));
    covered[start.y][start.x] = true;
    Position current = start;

    // 单元覆盖顺序：从起点单元出发做「邻接贪心 DFS」，无未访问邻接时全局
    // 跳到离起点最近（曼哈顿）的未访问单元。排序用曼哈顿距离作启发式，
    // 实际路径质量由下方覆盖阶段的「入口+出口」一步前瞻保证。
    std::vector<int> order;
    order.reserve(cells.size());
    std::vector<bool> cellVisited(cells.size(), false);
    {
        int cur = startCell;
        cellVisited[cur] = true;
        order.push_back(cur);
        while (order.size() < cells.size()) {
            int best = -1;
            int bestDist = 1 << 30;
            for (const int nb : adj[cur]) {
                if (cellVisited[nb]) {
                    continue;
                }
                Position entry{};
                if (nearestUncoveredInCell(cells[nb], current, reachable, covered,
                                           entry)) {
                    const int d = manhattan(entry, current);
                    if (d < bestDist) {
                        bestDist = d;
                        best = nb;
                    }
                }
            }
            if (best < 0) {
                for (std::size_t ci = 0; ci < cells.size(); ++ci) {
                    if (cellVisited[ci]) {
                        continue;
                    }
                    Position entry{};
                    if (nearestUncoveredInCell(cells[ci], current, reachable,
                                               covered, entry)) {
                        const int d = manhattan(entry, current);
                        if (d < bestDist) {
                            bestDist = d;
                            best = static_cast<int>(ci);
                        }
                    }
                }
            }
            if (best < 0) {
                break;
            }
            cellVisited[best] = true;
            order.push_back(best);
            cur = best;
        }
    }

    // 按顺序覆盖每个单元：4 种蛇形变体中选「入口距离 + 出口到下一单元的
    // 距离」最小者（一步前瞻），单元内逐格用 BFS 衔接。
    for (std::size_t oi = 0; oi < order.size(); ++oi) {
        const Cell& cell = cells[order[oi]];
        if (!cellHasUncovered(cell, reachable, covered)) {
            continue;
        }

        // 下一个「还有未覆盖格」的单元（用于出口前瞻）。
        int nextCell = -1;
        for (std::size_t k = oi + 1; k < order.size(); ++k) {
            if (cellHasUncovered(cells[order[k]], reachable, covered)) {
                nextCell = order[k];
                break;
            }
        }

        const std::vector<Position> variants[4] = {
            serpentineVertical(cell, /*downFirst=*/false),
            serpentineVertical(cell, /*downFirst=*/true),
            serpentineHorizontal(cell, /*rightFirst=*/false),
            serpentineHorizontal(cell, /*rightFirst=*/true),
        };

        int best = 0;
        int bestScore = 1 << 30;
        for (int v = 0; v < 4; ++v) {
            // 入口：变体中第一个「可达且未覆盖」格。
            Position entry{};
            bool hasEntry = false;
            for (const Position& p : variants[v]) {
                if (reachable[p.y][p.x] && !covered[p.y][p.x]) {
                    entry = p;
                    hasEntry = true;
                    break;
                }
            }
            if (!hasEntry) {
                continue;
            }
            const int entryDist = manhattan(entry, current);

            // 出口：变体中最后一个「可达且未覆盖」格。
            Position exit = entry;
            for (const Position& p : variants[v]) {
                if (reachable[p.y][p.x] && !covered[p.y][p.x]) {
                    exit = p;
                }
            }

            // 出口到下一单元最近未覆盖格的距离（一步前瞻）。
            int exitDist = 0;
            if (nextCell >= 0) {
                Position nextEntry{};
                if (nearestUncoveredInCell(cells[nextCell], exit, reachable,
                                           covered, nextEntry)) {
                    exitDist = manhattan(exit, nextEntry);
                }
            }

            const int score = entryDist + exitDist;
            if (score < bestScore) {
                bestScore = score;
                best = v;
            }
        }

        for (const Position& p : variants[best]) {
            if (covered[p.y][p.x] || !reachable[p.y][p.x]) {
                continue;
            }
            const int dx = p.x - current.x;
            const int dy = p.y - current.y;
            const int dist = std::abs(dx) + std::abs(dy);
            if (dist == 0) {
                continue;
            }
            if (dist == 1) {
                // 相邻步：直接单步推进，避免为每个相邻格做全图 BFS 分配
                // （单元内蛇形步曼哈顿距离恒为 1，占覆盖步数的绝大多数）。
                Direction dir;
                if (dx == 1) {
                    dir = Direction::Right;
                } else if (dx == -1) {
                    dir = Direction::Left;
                } else if (dy == 1) {
                    dir = Direction::Down;
                } else {
                    dir = Direction::Up;
                }
                path.push_back(dir);
                current = p;
                covered[p.y][p.x] = true;
                continue;
            }
            // 跨单元入口：BFS 最短路径衔接。
            const std::vector<Direction> dirs = bfsPath(grid, current, p);
            for (const Direction dir : dirs) {
                const Position d = delta(dir);
                path.push_back(dir);
                current = Position{current.x + d.x, current.y + d.y};
                covered[current.y][current.x] = true;
            }
        }
    }
    return path;
}

std::vector<Direction> planPatrolPathGreedy(const Grid& grid, Position start) {
    std::vector<Direction> path;
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

std::vector<Direction> planPatrolPath(const Grid& grid, Position start) {
    const std::vector<Direction> boustrophedon =
        planPatrolPathBoustrophedon(grid, start);
    const std::vector<Direction> greedy = planPatrolPathGreedy(grid, start);
    return boustrophedon.size() <= greedy.size() ? boustrophedon : greedy;
}

}  // namespace patrol
