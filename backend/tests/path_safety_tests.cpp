#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "api/path_safety.hpp"

namespace {

using patrol::path_safety::safeJoin;
using patrol::path_safety::isPathWithinRoot;

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

// ---- isPathWithinRoot：符号链接纵深防御的路径前缀校验 ----

// 自身与子孙路径在 root 之内。
TEST(PathSafety, IsPathWithinRootAcceptsSelfAndDescendants) {
    namespace fs = std::filesystem;
    EXPECT_TRUE(isPathWithinRoot(fs::path("/srv/web"), fs::path("/srv/web")));
    EXPECT_TRUE(
        isPathWithinRoot(fs::path("/srv/web"), fs::path("/srv/web/index.html")));
    EXPECT_TRUE(isPathWithinRoot(
        fs::path("/srv/web"), fs::path("/srv/web/assets/css/style.css")));
}

// root 之外（父目录/兄弟目录/完全无关）拒绝。注意 /srv/webserver 与 /srv/web
// 字符串前缀相同，但按组件比较应正确判定为兄弟目录而拒绝。
TEST(PathSafety, IsPathWithinRootRejectsOutside) {
    namespace fs = std::filesystem;
    EXPECT_FALSE(isPathWithinRoot(fs::path("/srv/web"), fs::path("/etc/passwd")));
    EXPECT_FALSE(
        isPathWithinRoot(fs::path("/srv/web"), fs::path("/srv/webserver")));
    EXPECT_FALSE(isPathWithinRoot(fs::path("/srv/web"), fs::path("/srv")));
    EXPECT_FALSE(isPathWithinRoot(fs::path("/srv/web"), fs::path("/")));
}

// 符号链接逃逸：staticRoot 内指向目录外的 symlink 必须被 canonical 校验拦下。
// 用真实 symlink 验证 serveStatic 的「weakly_canonical + isPathWithinRoot」流水线。
TEST(PathSafety, SymlinkEscapeIsBlockedByCanonicalization) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const auto stamp = std::chrono::high_resolution_clock::now()
                           .time_since_epoch()
                           .count();
    const fs::path base =
        fs::temp_directory_path(ec) / ("path_safety_" + std::to_string(stamp));
    if (ec) {
        GTEST_SKIP() << "temp directory unavailable";
    }
    const fs::path root = base / "root";
    const fs::path secret = base / "secret.txt";
    fs::create_directories(root, ec);
    if (ec) {
        GTEST_SKIP() << "cannot create test dirs";
    }
    {
        std::ofstream ofs(secret, std::ios::binary);
        ofs << "outside secret";
    }
    // root/link -> secret（base 下、root 之外的外部文件，绝对路径）。
    fs::create_symlink(secret, root / "link", ec);
    if (ec) {
        fs::remove_all(base, ec);
        GTEST_SKIP() << "symlink unsupported on this platform";
    }
    // 复刻 serveStatic 的校验：canonical 后仍需落在 root 之内。
    const fs::path realRoot = fs::weakly_canonical(root, ec);
    ASSERT_FALSE(ec) << "weakly_canonical(root) failed";
    const fs::path realPath = fs::weakly_canonical(root / "link", ec);
    ASSERT_FALSE(ec) << "weakly_canonical(root/link) failed";
    EXPECT_FALSE(isPathWithinRoot(realRoot, realPath));
    // 对照：root 内的普通文件应通过。
    {
        std::ofstream ofs(root / "ok.txt", std::ios::binary);
        ofs << "ok";
    }
    const fs::path realOk = fs::weakly_canonical(root / "ok.txt", ec);
    ASSERT_FALSE(ec);
    EXPECT_TRUE(isPathWithinRoot(realRoot, realOk));
    fs::remove_all(base, ec);
}

}  // namespace
