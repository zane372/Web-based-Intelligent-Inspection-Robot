#include "api/command_parser.hpp"

#include <nlohmann/json.hpp>

#include "dto.hpp"

namespace patrol {

CommandParseResult parseCommandBody(const std::string& body) {
    CommandParseResult result;

    // 1) JSON 解析。
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception&) {
        result.error = ParseError::MalformedJson;
        return result;
    }

    // 2) 根节点必须是对象。
    if (!root.is_object()) {
        result.error = ParseError::NotObject;
        return result;
    }

    // 3) type 字段存在且合法。
    if (!root.contains("type")) {
        result.error = ParseError::MissingType;
        result.detail = "type";
        return result;
    }
    if (!root["type"].is_string()) {
        result.error = ParseError::InvalidCommandType;
        result.detail = "type";
        return result;
    }
    const std::string type = root["type"].get<std::string>();
    if (type == "MOVE") {
        result.command.type = CommandType::Move;
    } else if (type == "RESET") {
        result.command.type = CommandType::Reset;
    } else if (type == "START_PATROL") {
        result.command.type = CommandType::StartPatrol;
    } else if (type == "STOP_PATROL") {
        result.command.type = CommandType::StopPatrol;
    } else {
        result.error = ParseError::InvalidCommandType;
        result.detail = "type";
        return result;
    }

    // 4) 未知字段检查（MOVE 允许 type/direction，其余命令仅允许 type）。
    const bool isMove = result.command.type == CommandType::Move;
    for (auto it = root.begin(); it != root.end(); ++it) {
        const std::string key = it.key();
        const bool known = key == "type" || (isMove && key == "direction");
        if (!known) {
            result.error = ParseError::UnknownField;
            result.detail = key;
            return result;
        }
    }

    // 5) MOVE 需要 direction。
    if (isMove) {
        if (!root.contains("direction")) {
            result.error = ParseError::MissingDirection;
            result.detail = "direction";
            return result;
        }
        if (!root["direction"].is_string()) {
            result.error = ParseError::InvalidDirection;
            result.detail = "direction";
            return result;
        }
        Direction direction{};
        if (!directionFromString(root["direction"].get<std::string>(), direction)) {
            result.error = ParseError::InvalidDirection;
            result.detail = "direction";
            return result;
        }
        result.command.direction = direction;
    }

    result.error = ParseError::Ok;
    return result;
}

}  // namespace patrol
