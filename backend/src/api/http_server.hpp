#pragma once

#include <string>

#include "config.hpp"
#include "control/robot_controller.hpp"
#include "perception/perception_service.hpp"

namespace patrol {

// 组装 Crow 路由并阻塞运行 HTTP 服务。
// - config：只读地图配置（mapId/宽高/初始姿态）。
// - controller：线程安全的机器人控制器，所有路由共享同一个实例。
// - perception：虚拟感知服务（enabled=false 时 latest 返回 observation=null）。
// - staticRoot：前端静态资源根目录（构建产物 dist 的容器内路径）。
// - host/port：监听地址与端口（由调用方传入；main.cpp 默认 127.0.0.1）。
void runHttpServer(const MapConfig& config,
                   RobotController& controller,
                   PerceptionService& perception,
                   const std::string& staticRoot,
                   const std::string& host,
                   int port);

}  // namespace patrol
