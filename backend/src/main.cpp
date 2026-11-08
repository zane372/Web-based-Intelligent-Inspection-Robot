#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <random>
#include <string>

#include "api/http_server.hpp"
#include "config.hpp"
#include "control/robot_controller.hpp"
#include "domain/grid.hpp"
#include "domain/robot.hpp"

namespace patrol {
namespace {

// 生成进程级实例标识：每次启动都不同，用于前端检测服务重启。
std::string makeInstanceId() {
    const std::uint64_t now = static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    std::random_device rd;
    std::mt19937_64 gen(rd());
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "srv-%016llx-%016llx",
                  static_cast<unsigned long long>(now),
                  static_cast<unsigned long long>(gen()));
    return buffer;
}

// 读取环境变量，缺省或为空时返回默认值。
std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? std::string(value) : fallback;
}

}  // namespace
}  // namespace patrol

int main() {
    using namespace patrol;

    const std::string mapPath = envOr("MAP_CONFIG", "config/map.json");
    const std::string staticRoot = envOr("STATIC_ROOT", "web");
    const std::string host = envOr("APP_HOST", "0.0.0.0");
    const int port = std::atoi(envOr("APP_PORT", "8080").c_str());

    // 配置加载失败按规格以非零码退出，并输出具体字段。
    MapConfig config;
    try {
        config = loadMapConfig(mapPath);
    } catch (const std::exception& e) {
        std::cerr << "[patrol_server] 配置加载失败: " << e.what() << '\n';
        return 1;
    }

    Grid grid(config.width, config.height);
    Robot robot(config.initialPosition, config.initialHeading);
    RobotController controller(grid, robot, "robot-1", makeInstanceId());

    std::cout << "[patrol_server] mapId=" << config.mapId
              << " size=" << config.width << 'x' << config.height
              << " initial=(" << config.initialPosition.x << ','
              << config.initialPosition.y << ") staticRoot=" << staticRoot
              << " listen=" << host << ':' << port << '\n';

    runHttpServer(config, controller, staticRoot, host, port);
    return 0;
}
