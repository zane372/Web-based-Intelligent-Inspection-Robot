#include <gtest/gtest.h>

#include <string>

#include "api/path_safety.hpp"

namespace {

using patrol::path_safety::safeJoin;

// 普通相对路径应安全拼接。
TEST(PathSafety, AcceptsPlainRelativePath) {
    std::string out;
    ASSERT_TRUE(safeJoin("/srv/web", "index.html", out));
    EXPECT_EQ(out, "/srv/web/index.html");
}

// 多层嵌套路径应正常。
TEST(PathSafety, AcceptsNestedPath) {
    std::string out;
    ASSERT_TRUE(safeJoin("/srv/web", "assets/css/style.css", out));
    EXPECT_EQ(out, "/srv/web/assets/css/style.css");
}

// 空路径拒绝。
TEST(PathSafety, RejectsEmptyPath) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "", out));
}

// 前导 / 的绝对路径拒绝。
TEST(PathSafety, RejectsLeadingSlash) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "/etc/passwd", out));
}

// "." 段拒绝（含裸 "."）。
TEST(PathSafety, RejectsDotSegment) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", ".", out));
    EXPECT_FALSE(safeJoin("/srv/web", "./x", out));
    EXPECT_FALSE(safeJoin("/srv/web", "a/./b", out));
}

// ".." 段拒绝（含裸 ".."）。
TEST(PathSafety, RejectsParentSegment) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "..", out));
    EXPECT_FALSE(safeJoin("/srv/web", "../x", out));
    EXPECT_FALSE(safeJoin("/srv/web", "a/../b", out));
}

// 前导反斜杠（绝对路径/UNC 前缀）拒绝。
TEST(PathSafety, RejectsLeadingBackslash) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "\\..\\..\\config\\map.json", out));
}

// 反斜杠构造的 .. 穿越拒绝。
TEST(PathSafety, RejectsBackslashParentTraversal) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "a\\..\\..\\config\\map.json", out));
}

// 盘符（C:\...、C:/...、C:）拒绝。
TEST(PathSafety, RejectsDriveLetter) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "C:\\Windows\\system.ini", out));
    EXPECT_FALSE(safeJoin("/srv/web", "C:/Windows/system.ini", out));
    EXPECT_FALSE(safeJoin("/srv/web", "C:", out));
}

// UNC 路径（\\server\share）拒绝。
TEST(PathSafety, RejectsUncPrefix) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "\\\\server\\share\\file", out));
}

// 空段（a//b、尾部斜杠）拒绝。
TEST(PathSafety, RejectsEmptySegment) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "a//b", out));
    EXPECT_FALSE(safeJoin("/srv/web", "a/", out));
}

// 含冒号的段（NTFS ADS，含非首段）拒绝。
TEST(PathSafety, RejectsColonInSegment) {
    std::string out;
    EXPECT_FALSE(safeJoin("/srv/web", "a:stream", out));
    EXPECT_FALSE(safeJoin("/srv/web", "a/b:c", out));
}

// 合法路径中的反斜杠规范化为正斜杠（含混合分隔符）。
TEST(PathSafety, NormalizesBackslashToForwardSlash) {
    std::string out;
    ASSERT_TRUE(safeJoin("/srv/web", "assets\\app.js", out));
    EXPECT_EQ(out, "/srv/web/assets/app.js");
    ASSERT_TRUE(safeJoin("/srv/web", "a\\b/c", out));
    EXPECT_EQ(out, "/srv/web/a/b/c");
}

}  // namespace
