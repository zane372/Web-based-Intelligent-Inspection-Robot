#include "config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "dto.hpp"

namespace patrol {

MapConfig loadMapConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("无法打开配置文件: " + path);
    }
    std::stringstream buffer;
    buffer << in.rdbuf();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(buffer.str());
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("配置 JSON 解析失败: ") + e.what());
    }
    if (!root.is_object()) {
        throw std::runtime_error("配置根节点必须是 JSON 对象");
    }

    if (!root.contains("mapId") || !root["mapId"].is_string() ||
        root["mapId"].get<std::string>().empty()) {
        throw std::runtime_error("配置缺少合法的 mapId");
    }
    if (!root.contains("width") || !root["width"].is_number_integer()) {
        throw std::runtime_error("配置缺少合法的 width（整数）");
    }
    if (!root.contains("height") || !root["height"].is_number_integer()) {
        throw std::runtime_error("配置缺少合法的 height（整数）");
    }

    const int width = root["width"].get<int>();
    const int height = root["height"].get<int>();
    if (width < 1 || width > 100) {
        throw std::runtime_error("width 必须在 [1, 100] 范围内");
    }
    if (height < 1 || height > 100) {
        throw std::runtime_error("height 必须在 [1, 100] 范围内");
    }

    if (!root.contains("initialRobot") || !root["initialRobot"].is_object()) {
        throw std::runtime_error("配置缺少 initialRobot 对象");
    }
    const nlohmann::json& ir = root["initialRobot"];
    if (!ir.contains("x") || !ir["x"].is_number_integer()) {
        throw std::runtime_error("initialRobot.x 必须是整数");
    }
    if (!ir.contains("y") || !ir["y"].is_number_integer()) {
        throw std::runtime_error("initialRobot.y 必须是整数");
    }
    if (!ir.contains("heading") || !ir["heading"].is_string()) {
        throw std::runtime_error("initialRobot.heading 必须是字符串");
    }

    const Position initialPosition{ir["x"].get<int>(), ir["y"].get<int>()};
    Direction initialHeading{};
    if (!directionFromString(ir["heading"].get<std::string>(), initialHeading)) {
        throw std::runtime_error("initialRobot.heading 非法: " +
                                 ir["heading"].get<std::string>());
    }
    if (initialPosition.x < 0 || initialPosition.x >= width ||
        initialPosition.y < 0 || initialPosition.y >= height) {
        throw std::runtime_error("initialRobot 初始位置越界");
    }

    // 可选 obstacles 数组：每个元素 {x, y}，须在界内且不与初始位置重合。
    std::vector<Position> obstacles;
    if (root.contains("obstacles")) {
        if (!root["obstacles"].is_array()) {
            throw std::runtime_error("obstacles 必须是数组");
        }
        for (const nlohmann::json& o : root["obstacles"]) {
            if (!o.is_object() || !o.contains("x") || !o["x"].is_number_integer() ||
                !o.contains("y") || !o["y"].is_number_integer()) {
                throw std::runtime_error("obstacles 元素必须是 {x, y} 整数对象");
            }
            const Position pos{o["x"].get<int>(), o["y"].get<int>()};
            if (pos.x < 0 || pos.x >= width || pos.y < 0 || pos.y >= height) {
                throw std::runtime_error("obstacles 元素越界");
            }
            if (pos == initialPosition) {
                throw std::runtime_error("obstacles 不能与初始位置重合");
            }
            obstacles.push_back(pos);
        }
    }

    MapConfig config;
    config.mapId = root["mapId"].get<std::string>();
    config.width = width;
    config.height = height;
    config.initialPosition = initialPosition;
    config.initialHeading = initialHeading;
    config.obstacles = std::move(obstacles);
    return config;
}

}  // namespace patrol
