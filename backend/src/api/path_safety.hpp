#pragma once

#include <string>

namespace patrol {
namespace path_safety {

// 判断字符是否为路径分隔符：/ 与 \ 均视为分隔符。
// Windows 上反斜杠同样是路径分隔符，仅按 / 切分会漏掉 \..\ 这类穿越。
inline bool isPathSeparator(char c) {
    return c == '/' || c == '\\';
}

// 校验 URL 相对路径安全并拼接文件系统路径。
// 拒绝：空路径、以分隔符开头的绝对路径（含盘符 C:、UNC \\server\share）、
//       "." / ".." 段、空段、含冒号的段（盘符、NTFS ADS 数据流）。
// 成功返回 true 并把结果写入 out；失败返回 false，调用方应拒绝请求。
inline bool safeJoin(const std::string& root, const std::string& urlPath,
                     std::string& out) {
    if (urlPath.empty() || isPathSeparator(urlPath[0])) {
        return false;
    }
    out = root;
    std::size_t start = 0;
    while (start <= urlPath.size()) {
        // 把 / 与 \ 都当作段分隔符，避免 Windows 上反斜杠构造的穿越段。
        const std::size_t sep = urlPath.find_first_of("/\\", start);
        const std::string segment = urlPath.substr(
            start, sep == std::string::npos ? std::string::npos : sep - start);
        if (segment.empty() || segment == "." || segment == ".." ||
            segment.find(':') != std::string::npos) {
            return false;
        }
        out += '/';
        out += segment;
        if (sep == std::string::npos) {
            break;
        }
        start = sep + 1;
    }
    return true;
}

}  // namespace path_safety
}  // namespace patrol
