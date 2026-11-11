#include "api/http_server.hpp"

#include <atomic>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "crow.h"

#include "api/command_parser.hpp"
#include "api/path_safety.hpp"
#include "dto.hpp"

// Crow 的默认路由只匹配 GET（routing.h 中 methods_ 初始化为 1<<Get）。
// 这里显式列出全部 HTTP 方法，使每个路由能命中任意方法，
// 再在 handler 内自行按方法返回 200 或 405（带 Allow 头）。
#define CROW_METHODS_ALL \
    crow::HTTPMethod::DELETE, crow::HTTPMethod::GET, crow::HTTPMethod::HEAD, \
    crow::HTTPMethod::POST, crow::HTTPMethod::PUT, crow::HTTPMethod::CONNECT, \
    crow::HTTPMethod::OPTIONS, crow::HTTPMethod::TRACE, crow::HTTPMethod::PATCH, \
    crow::HTTPMethod::PURGE, crow::HTTPMethod::COPY, crow::HTTPMethod::LOCK, \
    crow::HTTPMethod::MKCOL, crow::HTTPMethod::MOVE, crow::HTTPMethod::PROPFIND, \
    crow::HTTPMethod::PROPPATCH, crow::HTTPMethod::SEARCH, crow::HTTPMethod::UNLOCK, \
    crow::HTTPMethod::BIND, crow::HTTPMethod::REBIND, crow::HTTPMethod::UNBIND, \
    crow::HTTPMethod::ACL, crow::HTTPMethod::REPORT, crow::HTTPMethod::MKACTIVITY, \
    crow::HTTPMethod::CHECKOUT, crow::HTTPMethod::MERGE, crow::HTTPMethod::MSEARCH, \
    crow::HTTPMethod::NOTIFY, crow::HTTPMethod::SUBSCRIBE, \
    crow::HTTPMethod::UNSUBSCRIBE, crow::HTTPMethod::MKCALENDAR, \
    crow::HTTPMethod::LINK, crow::HTTPMethod::UNLINK, crow::HTTPMethod::SOURCE

namespace patrol {

namespace {

constexpr std::size_t kMaxBodyBytes = 4096;
// 静态文件单文件大小上限（8 MiB）：防止异常大的静态资源被整文件读入内存
// 造成内存放大；正常前端产物（HTML/CSS/JS/字体/图片）远小于此值。
constexpr std::streamsize kMaxStaticFileBytes = 8 * 1024 * 1024;

// 生成进程内唯一、不透明请求 ID（用于统一错误响应与日志关联）。
std::string makeRequestId() {
    static std::atomic<std::uint64_t> counter{0};
    const std::uint64_t n = counter.fetch_add(1, std::memory_order_relaxed);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "req-%08llx",
                  static_cast<unsigned long long>(n));
    return buffer;
}

// 去掉首尾空白并转小写，用于 Content-Type 媒体类型比较。
std::string lowercaseTrimmed(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    std::size_t begin = 0;
    std::size_t end = out.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(out[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(out[end - 1]))) {
        --end;
    }
    return out.substr(begin, end - begin);
}

// 判断 Content-Type 是否为 application/json（允许 charset 等参数）。
bool isJsonContentType(const std::string& contentType) {
    std::string value = lowercaseTrimmed(contentType);
    const std::size_t semicolon = value.find(';');
    if (semicolon != std::string::npos) {
        value = value.substr(0, semicolon);
    }
    value = lowercaseTrimmed(value);
    return value == "application/json";
}

// 构造 JSON 响应；head 为 true 时只返回头不返回体（供 HEAD 请求）。
crow::response jsonResponse(int code, const std::string& body, bool head) {
    crow::response res(code, head ? std::string{} : body);
    res.set_header("Content-Type", "application/json; charset=utf-8");
    return res;
}

crow::response jsonError(int status, const std::string& code,
                         const std::string& message,
                         const std::string& requestId,
                         const nlohmann::json& details = nlohmann::json::object()) {
    return jsonResponse(status, errorToJson(code, message, requestId, details).dump(),
                        false);
}

// 只读资源共用的方法检查：请求非 GET/HEAD 时返回已填充的 405 响应，
// 合法时返回 nullopt。调用方 `if (auto err = rejectIfNotGetOrHead(req)) return *err;`
// 即可避免在每个 GET 路由重复方法判断（SPEC 5.6 公共错误处理）。
std::optional<crow::response> rejectIfNotGetOrHead(const crow::request& req) {
    if (req.method != crow::HTTPMethod::GET &&
        req.method != crow::HTTPMethod::HEAD) {
        crow::response res = jsonError(405, "METHOD_NOT_ALLOWED",
                                       "资源不支持该 HTTP 方法", makeRequestId());
        res.set_header("Allow", "GET, HEAD");
        return res;
    }
    return std::nullopt;
}

// 严格十进制整数解析（SPEC 5.6：查询参数显式校验，禁止 atoi）。
// 空串 / 非数字字符 / 越界均返回 false。
bool parseStrictInt(const std::string& s, int& out) {
    if (s.empty()) {
        return false;
    }
    // 严格纯数字：拒绝前导空白与正负号（strtol 会接受 +5/前导空白，审查 M1）。
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    errno = 0;
    char* end = nullptr;
    const long value = std::strtol(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') {
        return false;
    }
    if (value < INT_MIN || value > INT_MAX) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

// 校验感知事件查询串：仅允许 after/limit 键且不得重复（SPEC 6.7，审查 M1）。
// 合法返回 true；非法将未知/重复字段名写入 badField。
bool validatePerceptionEventsQuery(const crow::request& req,
                                   std::string& badField) {
    // Crow 的 keys() 返回所有键（含重复，不含 '=' 及之后的值），
    // 据此校验：仅允许 after/limit 且各最多出现一次（审查 M1）。
    const std::vector<std::string> keys = req.url_params.keys();
    int afterCount = 0;
    int limitCount = 0;
    for (const std::string& key : keys) {
        if (key == "after") {
            ++afterCount;
        } else if (key == "limit") {
            ++limitCount;
        } else {
            badField = key;
            return false;
        }
    }
    if (afterCount > 1) {
        badField = "after";
        return false;
    }
    if (limitCount > 1) {
        badField = "limit";
        return false;
    }
    return true;
}

// 命令体解析错误码 -> HTTP 错误映射（均为 400）。
struct ErrorMapping {
    const char* code;
    const char* message;
};

ErrorMapping mapParseError(ParseError error) {
    switch (error) {
        case ParseError::MalformedJson:
            return {"MALFORMED_JSON", "请求体不是合法 JSON"};
        case ParseError::NotObject:
            return {"MALFORMED_JSON", "请求体必须是 JSON 对象"};
        case ParseError::MissingType:
            return {"MISSING_FIELD", "缺少必填字段 type"};
        case ParseError::InvalidCommandType:
            return {"INVALID_COMMAND_TYPE",
                    "type 必须是 MOVE/RESET/START_PATROL/STOP_PATROL 之一"};
        case ParseError::MissingDirection:
            return {"MISSING_FIELD", "MOVE 命令缺少必填字段 direction"};
        case ParseError::InvalidDirection:
            return {"INVALID_DIRECTION", "direction 必须是 UP/DOWN/LEFT/RIGHT 之一"};
        case ParseError::UnknownField:
            return {"UNKNOWN_FIELD", "存在未知字段"};
        case ParseError::Ok:
            return {"INTERNAL_ERROR", "不应出现"};
    }
    return {"INTERNAL_ERROR", "不应出现"};
}

// 命令序列号 -> commandId（8 位零填充）。
std::string makeCommandId(int sequence) {
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "cmd-%08d", sequence);
    return buffer;
}

// 由文件扩展名推断 MIME 类型。
std::string mimeTypeFor(const std::string& path) {
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }
    const std::string ext = path.substr(dot + 1);
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "js")   return "application/javascript; charset=utf-8";
    if (ext == "css")  return "text/css; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "svg")  return "image/svg+xml";
    if (ext == "png")  return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif")  return "image/gif";
    if (ext == "ico")  return "image/x-icon";
    if (ext == "woff") return "font/woff";
    if (ext == "woff2") return "font/woff2";
    return "application/octet-stream";
}

// 静态文件服务：命中返回文件内容，未命中返回 404。GET/HEAD 都读文件（封顶
// kMaxStaticFileBytes）；HEAD 的响应体由 Crow 依据 skip_body 自动剥除，
// Content-Length 仍按资源真实大小设置，故这里对 GET/HEAD 返回同一 body。
crow::response serveStatic(const std::string& root, const std::string& urlPath) {
    std::string fullPath;
    if (!path_safety::safeJoin(root, urlPath, fullPath)) {
        return crow::response(404, "not found");
    }
    std::ifstream in(fullPath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return crow::response(404, "not found");
    }
    // 先取文件大小，超过上限则拒绝，避免整文件读入内存。
    const std::streamoff size = in.tellg();
    if (size < 0 || size > kMaxStaticFileBytes) {
        return crow::response(413, "static file too large");
    }
    in.seekg(0, std::ios::beg);
    std::string body(static_cast<std::size_t>(size), '\0');
    if (size > 0) {
        in.read(body.data(), size);
        if (in.gcount() != size) {
            return crow::response(404, "not found");
        }
    }
    crow::response res(200, std::move(body));
    res.set_header("Content-Type", mimeTypeFor(fullPath));
    return res;
}

// 已知 API 路由及其允许的方法（用于 405 的 Allow 头）。
struct ApiRouteSpec {
    const char* path;
    const char* allow;
};

const ApiRouteSpec kApiRoutes[] = {
    {"/api/v1/health", "GET, HEAD"},
    {"/api/v1/map", "GET, HEAD"},
    {"/api/v1/robot", "GET, HEAD"},
    {"/api/v1/robot/commands", "POST"},
    {"/api/v1/perception/latest", "GET, HEAD"},
    {"/api/v1/perception/events", "GET, HEAD"},
};

const ApiRouteSpec* findApiRoute(const std::string& fullPath) {
    for (const auto& spec : kApiRoutes) {
        if (fullPath == spec.path) {
            return &spec;
        }
    }
    return nullptr;
}

}  // namespace

void runHttpServer(const MapConfig& config,
                   RobotController& controller,
                   PerceptionService& perception,
                   const std::string& staticRoot,
                   const std::string& host,
                   int port) {
    crow::SimpleApp app;

    // ---- GET /api/v1/health ----
    CROW_ROUTE(app, "/api/v1/health")
    .methods(CROW_METHODS_ALL)
    ([&controller](const crow::request& req) {
        if (auto err = rejectIfNotGetOrHead(req)) return std::move(*err);
        const bool head = (req.method == crow::HTTPMethod::HEAD);
        const std::string body =
            nlohmann::json{{"status", "ok"}, {"instanceId", controller.snapshot().instanceId}}.dump();
        return jsonResponse(200, body, head);
    });

    // ---- GET /api/v1/map ----
    CROW_ROUTE(app, "/api/v1/map")
    .methods(CROW_METHODS_ALL)
    ([&config](const crow::request& req) {
        if (auto err = rejectIfNotGetOrHead(req)) return std::move(*err);
        const bool head = (req.method == crow::HTTPMethod::HEAD);
        return jsonResponse(
            200,
            mapInfoToJson(config.mapId, config.width, config.height, config.obstacles)
                .dump(),
            head);
    });

    // ---- GET /api/v1/robot ----
    CROW_ROUTE(app, "/api/v1/robot")
    .methods(CROW_METHODS_ALL)
    ([&controller](const crow::request& req) {
        if (auto err = rejectIfNotGetOrHead(req)) return std::move(*err);
        const bool head = (req.method == crow::HTTPMethod::HEAD);
        return jsonResponse(200, robotSnapshotToJson(controller.snapshot()).dump(),
                            head);
    });

    // ---- GET /api/v1/perception/latest ----
    CROW_ROUTE(app, "/api/v1/perception/latest")
    .methods(CROW_METHODS_ALL)
    ([&perception](const crow::request& req) {
        if (auto err = rejectIfNotGetOrHead(req)) return std::move(*err);
        const bool head = (req.method == crow::HTTPMethod::HEAD);
        return jsonResponse(
            200, perceptionLatestToJson(perception.latest()).dump(), head);
    });

    // ---- GET /api/v1/perception/events ----
    CROW_ROUTE(app, "/api/v1/perception/events")
    .methods(CROW_METHODS_ALL)
    ([&perception](const crow::request& req) {
        if (auto err = rejectIfNotGetOrHead(req)) return std::move(*err);
        const bool head = (req.method == crow::HTTPMethod::HEAD);
        const std::string requestId = makeRequestId();

        // 查询参数显式校验（SPEC 6.7：after≥0 默认0，limit 1..100 默认50）。
        int after = 0;
        int limit = 50;
        std::string badField;
        if (!validatePerceptionEventsQuery(req, badField)) {
            return jsonError(400, "INVALID_QUERY_PARAMETER",
                             "查询参数非法（重复或未知字段）", requestId,
                             {{"field", badField}});
        }
        if (const char* afterParam = req.url_params.get("after")) {
            if (!parseStrictInt(afterParam, after) || after < 0) {
                return jsonError(400, "INVALID_QUERY_PARAMETER",
                                 "after 必须是非负整数", requestId,
                                 {{"field", "after"}});
            }
        }
        if (const char* limitParam = req.url_params.get("limit")) {
            if (!parseStrictInt(limitParam, limit) || limit < 1 || limit > 100) {
                return jsonError(400, "INVALID_QUERY_PARAMETER",
                                 "limit 必须在 [1, 100] 范围内", requestId,
                                 {{"field", "limit"}});
            }
        }
        return jsonResponse(
            200, perceptionEventsToJson(perception.events(after, limit)).dump(),
            head);
    });

    // ---- POST /api/v1/robot/commands ----
    CROW_ROUTE(app, "/api/v1/robot/commands")
    .methods(CROW_METHODS_ALL)
    ([&controller](const crow::request& req) {
        const std::string requestId = makeRequestId();

        if (req.method != crow::HTTPMethod::POST) {
            crow::response res = jsonError(405, "METHOD_NOT_ALLOWED",
                                           "资源不支持该 HTTP 方法", requestId);
            res.set_header("Allow", "POST");
            return res;
        }

        // 1) Content-Type 检查。
        if (!isJsonContentType(req.get_header_value("Content-Type"))) {
            return jsonError(415, "UNSUPPORTED_MEDIA_TYPE",
                             "Content-Type 必须是 application/json", requestId);
        }

        // 2) 请求体大小检查。
        if (req.body.size() > kMaxBodyBytes) {
            return jsonError(413, "PAYLOAD_TOO_LARGE",
                             "请求体超过 4096 字节上限", requestId);
        }

        // 3) 解析并校验命令体。
        const CommandParseResult parsed = parseCommandBody(req.body);
        if (parsed.error != ParseError::Ok) {
            const ErrorMapping mapping = mapParseError(parsed.error);
            nlohmann::json details = nlohmann::json::object();
            if (!parsed.detail.empty()) {
                details["field"] = parsed.detail;
            }
            if (parsed.error == ParseError::InvalidDirection) {
                details["allowed"] = {"UP", "DOWN", "LEFT", "RIGHT"};
            }
            return jsonError(400, mapping.code, mapping.message, requestId, details);
        }

        // 4) 执行命令（线程安全，原子地更新状态与序列号）。
        const CommandOutcome outcome =
            controller.execute(parsed.command.type, parsed.command.direction);

        nlohmann::json body = {
            {"commandId", makeCommandId(outcome.robot.sequence)},
            {"type", commandTypeToString(parsed.command.type)},
            {"result", commandResultToString(outcome.result)},
            {"robot", robotSnapshotToJson(outcome.robot)},
        };
        return jsonResponse(201, body.dump(), false);
    });

    // ---- 根路径 -> index.html ----
    CROW_ROUTE(app, "/")
    .methods(CROW_METHODS_ALL)
    ([&staticRoot](const crow::request& req) {
        if (req.method != crow::HTTPMethod::GET &&
            req.method != crow::HTTPMethod::HEAD) {
            crow::response res(405, "method not allowed");
            res.set_header("Allow", "GET, HEAD");
            return res;
        }
        std::ifstream in(staticRoot + "/index.html", std::ios::binary);
        if (!in.is_open()) {
            return crow::response(404, "not found");
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        // GET/HEAD 均返回完整 body，HEAD 的响应体由 Crow 依据 skip_body 剥除，
        // 使 Content-Length 保持为 index.html 的真实大小（而非 0）。
        crow::response res(200, buffer.str());
        res.set_header("Content-Type", "text/html; charset=utf-8");
        return res;
    });

    // ---- 兜底路由：未知 API 路径与静态资源 ----
    CROW_ROUTE(app, "/<path>")
    .methods(CROW_METHODS_ALL)
    ([&staticRoot](const crow::request& req, std::string path) {
        // /api/ 前缀：未知路由返回 JSON 404；已知路由但方法不符返回 405。
        if (path.rfind("api/", 0) == 0) {
            const std::string fullPath = "/" + path;
            if (const ApiRouteSpec* spec = findApiRoute(fullPath)) {
                crow::response res = jsonError(405, "METHOD_NOT_ALLOWED",
                                               "资源不支持该 HTTP 方法",
                                               makeRequestId());
                res.set_header("Allow", spec->allow);
                return res;
            }
            return jsonError(404, "ROUTE_NOT_FOUND", "未找到该 API 路由",
                             makeRequestId());
        }
        // 非 API：静态文件（GET/HEAD），其余方法 405。
        if (req.method != crow::HTTPMethod::GET &&
            req.method != crow::HTTPMethod::HEAD) {
            crow::response res(405, "method not allowed");
            res.set_header("Allow", "GET, HEAD");
            return res;
        }
        return serveStatic(staticRoot, path);
    });

    app.port(port).bindaddr(host).multithreaded().run();
}

}  // namespace patrol
