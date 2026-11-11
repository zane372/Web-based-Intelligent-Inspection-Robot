#pragma once

// 虚拟感知纯规则（SPEC 6.3 / 6.4）：三向旋转测距 + 曼哈顿异常检测。
// 全部为无副作用纯函数，便于在无 Crow/Boost 环境下单独测试。

#include <algorithm>
#include <cstdlib>
#include <random>

#include "domain/grid.hpp"
#include "perception/perception_types.hpp"

namespace patrol {

// 相对方向 -> 绝对方向：屏幕坐标（x 右、y 下）下逆时针/顺时针旋转。
inline Direction rotateLeft(Direction d) {
    switch (d) {
        case Direction::Up:    return Direction::Left;
        case Direction::Down:  return Direction::Right;
        case Direction::Left:  return Direction::Down;
        case Direction::Right: return Direction::Up;
    }
    return Direction::Up;
}

inline Direction rotateRight(Direction d) {
    switch (d) {
        case Direction::Up:    return Direction::Right;
        case Direction::Down:  return Direction::Left;
        case Direction::Left:  return Direction::Up;
        case Direction::Right: return Direction::Down;
    }
    return Direction::Up;
}

// 单条射线：从 pos 沿 dir 逐格前进，最多 rangeCells 步；
// 首个界外格 -> BOUNDARY（距离=步数，无 cell）；
// 首个障碍格 -> OBSTACLE（距离 + 格）；
// 无命中 -> NONE。命中即停。
inline RangeReading castRay(const Grid& grid, const Position pos,
                            const Direction dir, const int rangeCells,
                            const RelativeDirection relative,
                            const std::string& sensorId) {
    RangeReading r;
    r.sensorId = sensorId;
    r.relativeDirection = relative;
    r.absoluteDirection = dir;
    r.hitType = HitType::None;
    const Position d = delta(dir);
    Position cur = pos;
    for (int step = 1; step <= rangeCells; ++step) {
        cur.x += d.x;
        cur.y += d.y;
        if (!grid.contains(cur)) {
            r.hitType = HitType::Boundary;
            r.distanceCells = step;
            return r;
        }
        if (grid.isObstacle(cur)) {
            r.hitType = HitType::Obstacle;
            r.distanceCells = step;
            r.cell = cur;
            return r;
        }
    }
    return r;
}

// 三向测距：front / left / right 随 heading 旋转。
inline std::vector<RangeReading> computeRangeReadings(
    const Grid& grid, const Position pos, const Direction heading,
    const int rangeCells) {
    std::vector<RangeReading> readings;
    readings.reserve(3);
    readings.push_back(castRay(grid, pos, heading, rangeCells,
                               RelativeDirection::Front, "range-front"));
    readings.push_back(castRay(grid, pos, rotateLeft(heading), rangeCells,
                               RelativeDirection::Left, "range-left"));
    readings.push_back(castRay(grid, pos, rotateRight(heading), rangeCells,
                               RelativeDirection::Right, "range-right"));
    return readings;
}

// 曼哈顿半径内检测异常源，结果按 hazard.id 升序。
inline std::vector<Detection> detectHazards(
    const std::vector<Hazard>& hazards, const Position pos,
    const int hazardRangeCells) {
    std::vector<Detection> detections;
    detections.reserve(hazards.size());
    for (const Hazard& h : hazards) {
        const int dist = std::abs(h.position.x - pos.x) +
                         std::abs(h.position.y - pos.y);
        if (dist <= hazardRangeCells) {
            Detection d;
            d.hazardId = h.id;
            d.hazardType = h.type;
            d.severity = h.severity;
            d.label = h.label;
            d.position = h.position;
            d.distanceCells = dist;
            detections.push_back(std::move(d));
        }
    }
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.hazardId < b.hazardId;
              });
    return detections;
}

// 为每个异常源从可通行格中抽取互不重复的随机位置（RESET 后随机化真值）。
// 确定性 Fisher-Yates 洗牌（uniform_int_distribution，无模偏置），可注入固定种子以便测试；
// 结果保证界内可通行、互不重复。可通行格数少于 hazards.size()（含空网格）
// 时无法满足互不重复，返回空向量，由调用方保持原位。
inline std::vector<Position> sampleHazardPositions(
    const std::vector<Hazard>& hazards, const Grid& grid, std::mt19937& rng) {
    std::vector<Position> walkable;
    walkable.reserve(static_cast<std::size_t>(grid.width()) *
                     static_cast<std::size_t>(grid.height()));
    for (int y = 0; y < grid.height(); ++y) {
        for (int x = 0; x < grid.width(); ++x) {
            const Position p{x, y};
            if (grid.isWalkable(p)) {
                walkable.push_back(p);
            }
        }
    }
    if (walkable.size() < hazards.size()) {
        return {};  // 可通行格不足，无法保证互不重复。
    }
    for (std::size_t i = walkable.size(); i > 1; --i) {
        const std::size_t j =
            std::uniform_int_distribution<std::size_t>(0, i - 1)(rng);
        std::swap(walkable[i - 1], walkable[j]);
    }
    std::vector<Position> out;
    out.reserve(hazards.size());
    for (std::size_t i = 0; i < hazards.size(); ++i) {
        out.push_back(walkable[i]);
    }
    return out;
}

}  // namespace patrol
