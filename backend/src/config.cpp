#include "config.hpp"

#include <algorithm>
#include <fstream>
#include <set>
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

namespace {

bool hazardTypeFromString(const std::string& s, HazardType& out) {
    if (s == "SMOKE") {
        out = HazardType::Smoke;
        return true;
    }
    if (s == "HIGH_TEMP") {
        out = HazardType::HighTemp;
        return true;
    }
    if (s == "GAS_LEAK") {
        out = HazardType::GasLeak;
        return true;
    }
    return false;
}

bool severityFromString(const std::string& s, Severity& out) {
    if (s == "LOW") {
        out = Severity::Low;
        return true;
    }
    if (s == "MEDIUM") {
        out = Severity::Medium;
        return true;
    }
    if (s == "HIGH") {
        out = Severity::High;
        return true;
    }
    return false;
}

// 对象是否含任何已知键之外的字段（SPEC 6.2：未知字段拒绝）。
bool hasUnknownKey(const nlohmann::json& obj,
                   std::initializer_list<const char*> known) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        bool found = false;
        for (const char* k : known) {
            if (it.key() == k) {
                found = true;
                break;
            }
        }
        if (!found) {
            return true;
        }
    }
    return false;
}

}  // namespace

PerceptionConfig loadPerceptionConfig(const std::string& path,
                                      const MapConfig& map) {
    std::ifstream in(path);
    if (!in.is_open()) {
        throw std::runtime_error("无法打开感知配置: " + path);
    }
    std::stringstream buffer;
    buffer << in.rdbuf();

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(buffer.str());
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error(std::string("感知配置 JSON 解析失败: ") +
                                 e.what());
    }
    if (!root.is_object()) {
        throw std::runtime_error("感知配置根节点必须是 JSON 对象");
    }
    if (hasUnknownKey(root, {"version", "enabled", "rangeCells",
                             "hazardRangeCells", "hazards"})) {
        throw std::runtime_error("感知配置含未知字段");
    }

    if (!root.contains("version") || !root["version"].is_number_integer() ||
        root["version"].get<int>() != 1) {
        throw std::runtime_error("感知配置 version 必须是整数 1");
    }
    if (!root.contains("enabled") || !root["enabled"].is_boolean()) {
        throw std::runtime_error("感知配置 enabled 必须是布尔值");
    }
    if (!root.contains("rangeCells") ||
        !root["rangeCells"].is_number_integer()) {
        throw std::runtime_error("感知配置 rangeCells 必须是整数");
    }
    if (!root.contains("hazardRangeCells") ||
        !root["hazardRangeCells"].is_number_integer()) {
        throw std::runtime_error("感知配置 hazardRangeCells 必须是整数");
    }

    const int rangeCells = root["rangeCells"].get<int>();
    const int hazardRangeCells = root["hazardRangeCells"].get<int>();
    if (rangeCells < 1 || rangeCells > 20) {
        throw std::runtime_error("rangeCells 必须在 [1, 20] 范围内");
    }
    if (hazardRangeCells < 0 || hazardRangeCells > 20) {
        throw std::runtime_error("hazardRangeCells 必须在 [0, 20] 范围内");
    }

    if (!root.contains("hazards") || !root["hazards"].is_array()) {
        throw std::runtime_error("感知配置缺少 hazards 数组");
    }
    if (root["hazards"].size() > 128) {
        throw std::runtime_error("hazards 数量不能超过 128");
    }

    const auto isObstacle = [&](Position p) {
        return std::find(map.obstacles.begin(), map.obstacles.end(), p) !=
               map.obstacles.end();
    };

    std::vector<Hazard> hazards;
    std::set<std::string> seenIds;
    for (const nlohmann::json& h : root["hazards"]) {
        if (!h.is_object() ||
            hasUnknownKey(h, {"id", "type", "severity", "position", "label"})) {
            throw std::runtime_error("hazards 元素必须是含 id/type/severity/position/label 的对象");
        }
        if (!h.contains("id") || !h["id"].is_string() ||
            h["id"].get<std::string>().empty()) {
            throw std::runtime_error("hazards 元素缺少合法 id");
        }
        const std::string id = h["id"].get<std::string>();
        if (!seenIds.insert(id).second) {
            throw std::runtime_error("hazards 元素 id 重复: " + id);
        }
        if (!h.contains("type") || !h["type"].is_string()) {
            throw std::runtime_error("hazards 元素 type 必须是字符串");
        }
        HazardType type{};
        if (!hazardTypeFromString(h["type"].get<std::string>(), type)) {
            throw std::runtime_error("hazards 元素 type 非法（SMOKE/HIGH_TEMP/GAS_LEAK）");
        }
        if (!h.contains("severity") || !h["severity"].is_string()) {
            throw std::runtime_error("hazards 元素 severity 必须是字符串");
        }
        Severity severity{};
        if (!severityFromString(h["severity"].get<std::string>(), severity)) {
            throw std::runtime_error("hazards 元素 severity 非法（LOW/MEDIUM/HIGH）");
        }
        if (!h.contains("position") || !h["position"].is_object()) {
            throw std::runtime_error("hazards 元素缺少 position 对象");
        }
        const nlohmann::json& pos = h["position"];
        if (!pos.contains("x") || !pos["x"].is_number_integer() ||
            !pos.contains("y") || !pos["y"].is_number_integer()) {
            throw std::runtime_error("hazards 元素 position 必须是 {x, y} 整数");
        }
        const Position hazardPos{pos["x"].get<int>(), pos["y"].get<int>()};
        if (hazardPos.x < 0 || hazardPos.x >= map.width || hazardPos.y < 0 ||
            hazardPos.y >= map.height) {
            throw std::runtime_error("hazards 元素 position 越界");
        }
        if (isObstacle(hazardPos)) {
            throw std::runtime_error("hazards 元素 position 不可位于障碍格");
        }
        // label 可选非空字符串（SPEC 6.2），最长 80 个 Unicode 字符。
        std::string label;
        if (h.contains("label")) {
            if (!h["label"].is_string()) {
                throw std::runtime_error("hazards 元素 label 必须是字符串");
            }
            label = h["label"].get<std::string>();
            if (label.empty()) {
                throw std::runtime_error("hazards 元素 label 不能为空字符串");
            }
            if (label.size() > 80) {
                throw std::runtime_error("hazards 元素 label 长度不能超过 80");
            }
        }

        Hazard hazard;
        hazard.id = id;
        hazard.type = type;
        hazard.severity = severity;
        hazard.position = hazardPos;
        hazard.label = label;
        hazards.push_back(std::move(hazard));
    }

    PerceptionConfig config;
    config.enabled = root["enabled"].get<bool>();
    config.rangeCells = rangeCells;
    config.hazardRangeCells = hazardRangeCells;
    config.hazards = std::move(hazards);
    return config;
}

}  // namespace patrol
