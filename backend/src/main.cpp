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
#include "perception/perception_service.hpp"

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
    const std::string host = envOr("APP_HOST", "127.0.0.1");
    const int port = std::atoi(envOr("APP_PORT", "8080").c_str());

    // 配置加载失败按规格以非零码退出，并输出具体字段。
    MapConfig config;
    try {
        config = loadMapConfig(mapPath);
    } catch (const std::exception& e) {
        std::cerr << "[patrol_server] 配置加载失败: " << e.what() << '\n';
        return 1;
    }

    Grid grid(config.width, config.height, config.obstacles);
    Robot robot(config.initialPosition, config.initialHeading);
    RobotController controller(grid, robot, "robot-1", makeInstanceId());

    // 虚拟感知：未设 PERCEPTION_CONFIG 时关闭；非法配置非零退出（SPEC 6.2）。
    PerceptionConfig perceptionConfig;
    const std::string perceptionPath = envOr("PERCEPTION_CONFIG", "");
    if (!perceptionPath.empty()) {
        try {
            perceptionConfig = loadPerceptionConfig(perceptionPath, config);
        } catch (const std::exception& e) {
            std::cerr << "[patrol_server] 感知配置加载失败: " << e.what() << '\n';
            return 1;
        }
    }
    PerceptionService perception(perceptionConfig, controller.snapshot().instanceId,
                                 grid);

    // 接线：每次命令执行 / 巡检步推进后，以已确认快照通知感知服务
    // （观察在控制器锁外完成，不嵌套感知锁，AR-CON-006/007）。
    controller.setObserver(
        [&perception](const RobotSnapshot& snap) {
            perception.observe(snap.instanceId, snap.sequence, snap.position,
                               snap.heading, snap.lastResult);
        });

    // 初态观察（sequence=0，SPEC 6.5）。
    {
        const RobotSnapshot initial = controller.snapshot();
        perception.observe(initial.instanceId, initial.sequence,
                           initial.position, initial.heading, initial.lastResult);
    }

    std::cout << "[patrol_server] mapId=" << config.mapId
              << " size=" << config.width << 'x' << config.height
              << " obstacles=" << config.obstacles.size()
              << " initial=(" << config.initialPosition.x << ','
              << config.initialPosition.y << ") staticRoot=" << staticRoot
              << " listen=" << host << ':' << port
              << " perception=" << (perceptionConfig.enabled ? "enabled" : "disabled")
              << '\n';

    runHttpServer(config, controller, perception, staticRoot, host, port);
    return 0;
}
