// 命令体解析单元测试：覆盖 SIM-11 API 校验规则在边界层的实现。
#include <gtest/gtest.h>

#include "api/command_parser.hpp"

namespace patrol {
namespace {

TEST(CommandParserTest, ValidMove) {
    const CommandParseResult r = parseCommandBody(R"({"type":"MOVE","direction":"UP"})");
    EXPECT_EQ(r.error, ParseError::Ok);
    EXPECT_EQ(r.command.type, CommandType::Move);
    EXPECT_EQ(r.command.direction, Direction::Up);
}

TEST(CommandParserTest, ValidResetWithoutDirection) {
    const CommandParseResult r = parseCommandBody(R"({"type":"RESET"})");
    EXPECT_EQ(r.error, ParseError::Ok);
    EXPECT_EQ(r.command.type, CommandType::Reset);
}

// RESET 忽略 direction 字段（direction 是已知字段，不算未知）。
TEST(CommandParserTest, ResetIgnoresDirection) {
    const CommandParseResult r =
        parseCommandBody(R"({"type":"RESET","direction":"LEFT"})");
    EXPECT_EQ(r.error, ParseError::Ok);
    EXPECT_EQ(r.command.type, CommandType::Reset);
}

TEST(CommandParserTest, MalformedJson) {
    const CommandParseResult r = parseCommandBody("{not json");
    EXPECT_EQ(r.error, ParseError::MalformedJson);
}

TEST(CommandParserTest, NonObjectRoot) {
    EXPECT_EQ(parseCommandBody("[]").error, ParseError::NotObject);
    EXPECT_EQ(parseCommandBody("\"MOVE\"").error, ParseError::NotObject);
    EXPECT_EQ(parseCommandBody("42").error, ParseError::NotObject);
}

TEST(CommandParserTest, MissingType) {
    const CommandParseResult r = parseCommandBody(R"({"direction":"UP"})");
    EXPECT_EQ(r.error, ParseError::MissingType);
}

TEST(CommandParserTest, InvalidCommandType) {
    EXPECT_EQ(parseCommandBody(R"({"type":"JUMP"})").error,
              ParseError::InvalidCommandType);
    EXPECT_EQ(parseCommandBody(R"({"type":42})").error,
              ParseError::InvalidCommandType);
}

TEST(CommandParserTest, UnknownField) {
    const CommandParseResult r =
        parseCommandBody(R"({"type":"MOVE","direction":"UP","speed":10})");
    EXPECT_EQ(r.error, ParseError::UnknownField);
    EXPECT_EQ(r.detail, "speed");
}

TEST(CommandParserTest, MoveMissingDirection) {
    const CommandParseResult r = parseCommandBody(R"({"type":"MOVE"})");
    EXPECT_EQ(r.error, ParseError::MissingDirection);
}

TEST(CommandParserTest, InvalidDirection) {
    EXPECT_EQ(parseCommandBody(R"({"type":"MOVE","direction":"NORTH"})").error,
              ParseError::InvalidDirection);
    EXPECT_EQ(parseCommandBody(R"({"type":"MOVE","direction":1})").error,
              ParseError::InvalidDirection);
}

}  // namespace
}  // namespace patrol
