#pragma once

#include <string>

#include "domain/types.hpp"

namespace patrol {

// 从 config/map.json 读取并校验后的地图与初始姿态。
struct MapConfig {
    std::string mapId;
    int width{0};
    int height{0};
    Position initialPosition;
    Direction initialHeading{Direction::Right};
};

// 读取并校验配置文件。校验失败抛 std::runtime_error，消息注明具体字段。
MapConfig loadMapConfig(const std::string& path);

}  // namespace patrol
