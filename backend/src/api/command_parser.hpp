#pragma once

#include <string>

#include "control/robot_controller.hpp"

namespace patrol {

// 命令体校验结果。校验失败不改变任何状态，也不占用序列号。
enum class ParseError {
    Ok,
    MalformedJson,
    NotObject,
    MissingType,
    InvalidCommandType,
    MissingDirection,
    InvalidDirection,
    UnknownField,
};

// 解析成功后的命令（direction 仅对 Move 有意义）。
struct ParsedCommand {
    CommandType type{CommandType::Move};
    Direction direction{Direction::Right};
};

// 命令解析结果：错误码 + 便于定位的错误字段/值 + 解析出的命令。
struct CommandParseResult {
    ParseError error{ParseError::Ok};
    std::string detail;  // 非法字段名，用于错误响应 details
    ParsedCommand command;
};

// 按规格顺序校验 POST /api/v1/robot/commands 的 JSON 请求体：
// 解析 → 对象检查 → type 存在/合法 → 未知字段 → MOVE 时 direction 存在/合法。
CommandParseResult parseCommandBody(const std::string& body);

}  // namespace patrol
