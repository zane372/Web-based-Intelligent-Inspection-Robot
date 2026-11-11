#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
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

// 解析端口号：仅接受 1..65535 的纯十进制数字，非法返回 false。
// 逐位累加并在每次累加后立即检查上界，避免 atoi 的静默截断与溢出。
bool parsePort(const std::string& text, int& out) {
    if (text.empty()) {
        return false;
    }
    int value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
        if (value > 65535) {
            return false;
        }
    }
    if (value < 1) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace
}  // namespace patrol

int main() {
    using namespace patrol;

    const std::string mapPath = envOr("MAP_CONFIG", "config/map.json");
    const std::string staticRoot = envOr("STATIC_ROOT", "web");
    const std::string host = envOr("APP_HOST", "127.0.0.1");
    int port = 0;
    if (!parsePort(envOr("APP_PORT", "8080"), port)) {
        std::cerr << "[patrol_server] APP_PORT 非法: 需为 1..65535 的十进制整数\n";
        return 1;
    }

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
    // 用 shared_ptr 持有感知服务：controller 的 observer lambda 在巡检线程中
    // 于控制器锁外被调用；controller 声明早于 perception，析构时 perception
    // 先亡，若 lambda 按引用捕获 &perception，则 controller 析构 join 巡检线程
    // 期间线程再调 observer 会访问已析构对象（UAF）。按值捕获 shared_ptr 使
    // 感知服务存活至最后一个 observer 调用结束。
    auto perception = std::make_shared<PerceptionService>(
        perceptionConfig, controller.snapshot().instanceId, grid);

    // 接线：每次命令执行 / 巡检步推进后，以已确认快照通知感知服务
    // （观察在控制器锁外完成，不嵌套感知锁，AR-CON-006/007）。
    controller.setObserver(
        [perception](const RobotSnapshot& snap) {
            perception->observe(snap.instanceId, snap.sequence, snap.position,
                                snap.heading, snap.lastResult);
        });

    // 初态观察（sequence=0，SPEC 6.5）。
    {
        const RobotSnapshot initial = controller.snapshot();
        perception->observe(initial.instanceId, initial.sequence,
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

    runHttpServer(config, controller, *perception, staticRoot, host, port);
    return 0;
}
