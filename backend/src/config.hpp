#pragma once

#include <string>
#include <vector>

#include "domain/types.hpp"
#include "perception/perception_types.hpp"

namespace patrol {

// 从 config/map.json 读取并校验后的地图与初始姿态。
struct MapConfig {
    std::string mapId;
    int width{0};
    int height{0};
    Position initialPosition;
    Direction initialHeading{Direction::Right};
    // 障碍物单元格（可选，缺省为空表示无障碍）。
    std::vector<Position> obstacles;
};

// 读取并校验配置文件。校验失败抛 std::runtime_error，消息注明具体字段。
MapConfig loadMapConfig(const std::string& path);

// 读取并校验虚拟感知配置（SPEC 6.2）。map 用于校验异常源位置「界内可通行」。
// 校验失败抛 std::runtime_error，消息注明具体字段；非法配置进程非零退出。
PerceptionConfig loadPerceptionConfig(const std::string& path,
                                      const MapConfig& map);

}  // namespace patrol
