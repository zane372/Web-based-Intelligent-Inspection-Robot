#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "config.hpp"

namespace patrol {
namespace {

// 写临时 JSON 配置文件，返回路径。
std::string writeTempJson(const std::string& name, const std::string& content) {
    const std::string path = std::string("tmp_") + name + ".json";
    std::ofstream out(path);
    out << content;
    out.close();
    return path;
}

MapConfig makeMap() {
    MapConfig m;
    m.mapId = "grid-10x10";
    m.width = 10;
    m.height = 10;
    m.initialPosition = Position{0, 0};
    m.initialHeading = Direction::Right;
    m.obstacles = {Position{4, 4}};
    return m;
}

TEST(PerceptionConfig, ValidConfigWithLabel) {
    const std::string path = writeTempJson(
        "valid_label",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"HIGH",
                        "position":{"x":1,"y":1},"label":"烟雾"}]})");
    const auto cfg = loadPerceptionConfig(path, makeMap());
    EXPECT_TRUE(cfg.enabled);
    EXPECT_EQ(cfg.rangeCells, 3);
    EXPECT_EQ(cfg.hazardRangeCells, 2);
    ASSERT_EQ(cfg.hazards.size(), 1u);
    EXPECT_EQ(cfg.hazards[0].id, "h1");
    EXPECT_EQ(cfg.hazards[0].type, HazardType::Smoke);
    EXPECT_EQ(cfg.hazards[0].severity, Severity::High);
    EXPECT_EQ(cfg.hazards[0].label, "烟雾");
    std::remove(path.c_str());
}

TEST(PerceptionConfig, ValidConfigWithoutLabel) {
    // label 可选（SPEC 6.2）。
    const std::string path = writeTempJson(
        "valid_nolabel",
        R"({"version":1,"enabled":false,"rangeCells":1,"hazardRangeCells":0,
            "hazards":[{"id":"h1","type":"GAS_LEAK","severity":"LOW",
                        "position":{"x":1,"y":1}}]})");
    const auto cfg = loadPerceptionConfig(path, makeMap());
    EXPECT_FALSE(cfg.enabled);
    ASSERT_EQ(cfg.hazards.size(), 1u);
    EXPECT_TRUE(cfg.hazards[0].label.empty());
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsUnknownField) {
    const std::string path = writeTempJson(
        "unknown",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[],"bogus":1})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsBadVersion) {
    const std::string path = writeTempJson(
        "badversion",
        R"({"version":2,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsOutOfRangeRangeCells) {
    const std::string path = writeTempJson(
        "badrange",
        R"({"version":1,"enabled":true,"rangeCells":21,"hazardRangeCells":2,
            "hazards":[]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsNegativeHazardRange) {
    const std::string path = writeTempJson(
        "badhazard",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":-1,
            "hazards":[]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsHazardOnObstacle) {
    const std::string path = writeTempJson(
        "onobstacle",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"HIGH",
                        "position":{"x":4,"y":4}}]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsHazardOutOfBounds) {
    const std::string path = writeTempJson(
        "oob",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"HIGH",
                        "position":{"x":99,"y":99}}]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsDuplicateId) {
    const std::string path = writeTempJson(
        "dupid",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"HIGH",
                        "position":{"x":1,"y":1}},
                       {"id":"h1","type":"GAS_LEAK","severity":"LOW",
                        "position":{"x":2,"y":2}}]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

TEST(PerceptionConfig, RejectsBadTypeAndSeverity) {
    const std::string path1 = writeTempJson(
        "badtype",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"FIRE","severity":"HIGH",
                        "position":{"x":1,"y":1}}]})");
    EXPECT_THROW(loadPerceptionConfig(path1, makeMap()), std::runtime_error);
    std::remove(path1.c_str());

    const std::string path2 = writeTempJson(
        "badseverity",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"CRITICAL",
                        "position":{"x":1,"y":1}}]})");
    EXPECT_THROW(loadPerceptionConfig(path2, makeMap()), std::runtime_error);
    std::remove(path2.c_str());
}

TEST(PerceptionConfig, RejectsOversizedLabel) {
    const std::string path = writeTempJson(
        "longlabel",
        R"({"version":1,"enabled":true,"rangeCells":3,"hazardRangeCells":2,
            "hazards":[{"id":"h1","type":"SMOKE","severity":"HIGH",
                        "position":{"x":1,"y":1},"label":")" +
            std::string(81, 'x') +
            R"("}]})");
    EXPECT_THROW(loadPerceptionConfig(path, makeMap()), std::runtime_error);
    std::remove(path.c_str());
}

}  // namespace
}  // namespace patrol
