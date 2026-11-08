#pragma once

#include <string>

namespace patrol {

// 位置：整数坐标 (x, y)。x 为列号向右递增，y 为行号向下递增，
// 原点 (0,0) 位于地图左上角。不表示像素坐标。
struct Position {
    int x{0};
    int y{0};

    bool operator==(const Position& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Position& other) const {
        return !(*this == other);
    }
    // 字典序比较，供有序容器（std::set/std::map）使用。
    bool operator<(const Position& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }
};

// 机器人朝向：四个方向，对应方向键。
enum class Direction {
    Up,
    Down,
    Left,
    Right,
};

// 运行状态：Idle 表示正常，Blocked 表示最近一次移动被边界阻止。
enum class MotionStatus {
    Idle,
    Blocked,
};

// 最近一次命令结果。
enum class LastResult {
    None,             // 尚未执行过任何命令（进程启动初态）
    Moved,            // 成功移动一格
    BlockedBoundary,  // 越界被阻止，位置不变
    BlockedObstacle,  // 前方为障碍物被阻止，位置不变
    Reset,            // 已重置到初始状态
    PatrolStarted,    // 自动巡检已启动
    PatrolStopped,    // 自动巡检被手动停止
    PatrolCompleted,  // 自动巡检已遍历完成
};

// 单次移动命令的结果（只用于移动，不包含 Reset）。
enum class MoveResult {
    Moved,
    BlockedBoundary,
    BlockedObstacle,
};

// 四方向坐标增量：x 向右、y 向下。
inline Position delta(Direction direction) {
    switch (direction) {
        case Direction::Up:    return {0, -1};
        case Direction::Down:  return {0, +1};
        case Direction::Left:  return {-1, 0};
        case Direction::Right: return {+1, 0};
    }
    return {0, 0};
}

}  // namespace patrol
