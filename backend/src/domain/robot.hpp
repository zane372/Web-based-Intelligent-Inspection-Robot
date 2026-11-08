#pragma once

#include "domain/grid.hpp"
#include "domain/types.hpp"

namespace patrol {

// 机器人领域模型：只执行一次离散移动规则与重置规则，
// 不依赖 HTTP、JSON 或 Canvas。序列号、instanceId 等由控制模块维护。
class Robot {
public:
    // initialPosition / initialHeading 为配置中的初始姿态。
    Robot(Position initialPosition, Direction initialHeading);

    // 按规格顺序执行一次移动：
    // 1) 先把朝向更新为命令方向；
    // 2) 计算候选位置；
    // 3) 越界则位置不变并标记 Blocked，否则移动到候选位置并恢复 Idle。
    MoveResult applyManualMove(Direction direction, const Grid& grid);

    // 恢复到给定初始姿态，运行状态恢复 Idle，最近结果记为 Reset。
    void reset(Position initialPosition, Direction initialHeading);

    Position position() const { return position_; }
    Direction heading() const { return heading_; }
    MotionStatus motionStatus() const { return motionStatus_; }
    LastResult lastResult() const { return lastResult_; }

private:
    Position position_;
    Direction heading_;
    MotionStatus motionStatus_;
    LastResult lastResult_;
};

}  // namespace patrol
