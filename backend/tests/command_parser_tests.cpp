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

// RESET 不允许携带 direction（SPEC 4.2.7）：应判为未知字段。
TEST(CommandParserTest, ResetRejectsDirection) {
    const CommandParseResult r =
        parseCommandBody(R"({"type":"RESET","direction":"LEFT"})");
    EXPECT_EQ(r.error, ParseError::UnknownField);
    EXPECT_EQ(r.detail, "direction");
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
    const CommandParseResult a =
        parseCommandBody(R"({"type":"MOVE","direction":"NORTH"})");
    EXPECT_EQ(a.error, ParseError::InvalidDirection);
    EXPECT_EQ(a.detail, "direction");
    const CommandParseResult b =
        parseCommandBody(R"({"type":"MOVE","direction":1})");
    EXPECT_EQ(b.error, ParseError::InvalidDirection);
    EXPECT_EQ(b.detail, "direction");
}

}  // namespace
}  // namespace patrol
