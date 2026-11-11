#pragma once

// 虚拟感知服务（SPEC 6.5 / 6.6）：观察去重、活动检测、事件环形缓冲。
// 只依赖 domain 与纯规则，不依赖 Crow / nlohmann/json / Boost。

#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "domain/grid.hpp"
#include "perception/perception_rules.hpp"
#include "perception/perception_types.hpp"

namespace patrol {

// GET /api/v1/perception/latest 的视图。
struct PerceptionLatestView {
    bool enabled{false};
    std::string instanceId;
    std::optional<PerceptionObservation> observation;
    std::vector<Detection> activeDetections;
    int latestEventId{0};
};

// GET /api/v1/perception/events 的视图。
struct PerceptionEventsView {
    std::string instanceId;
    std::vector<PerceptionEvent> events;
    int oldestEventId{0};   // 0 = 无（序列化为 null）
    int latestEventId{0};
    bool hasMore{false};
    bool truncated{false};
};

class PerceptionService {
public:
    PerceptionService(PerceptionConfig config, std::string instanceId,
                      Grid grid, std::uint32_t rngSeed = 0);

    // 观察一次机器人状态（SPEC 6.5）。线程安全；按 (instanceId, sequence)
    // 去重，忽略同实例下更旧的快照。lastResult 用于生成接触事件；
    // lastResult == Reset 时重新随机化异常源位置（SPEC 6.4 扩展）。
    void observe(const std::string& instanceId, int sequence, Position position,
                 Direction heading, LastResult lastResult);

    PerceptionLatestView latest() const;
    PerceptionEventsView events(int after, int limit) const;

    bool enabled() const { return config_.enabled; }

    // 当前异常源真值位置（仅测试/诊断读取；对外 API 不暴露真值）。
    std::vector<Position> hazardPositions() const;

private:
    static constexpr std::size_t kEventBufferCapacity = 200;

    void appendEventLocked(PerceptionEventType type, const std::string& instanceId,
                           int sequence, Position position,
                           const std::optional<Detection>& hazard,
                           const std::optional<LastResult>& contact);

    // 补记接触事件（CONTACT 是运动结果的边沿信号，不依赖活动检测集）。
    // 乱序观察被去重时也调用，避免并发命令「偷跑」更高序列时，巡检步的
    // 受阻接触事件被静默丢弃。
    void appendContactEventLocked(const std::string& instanceId, int sequence,
                                  Position position, LastResult lastResult);

    // RESET 后重新随机化 config_.hazards 的位置（须持有 mutex_）。
    void randomizeHazardsLocked();

    PerceptionConfig config_;
    std::string instanceId_;
    Grid grid_;
    std::mt19937 rng_;

    mutable std::mutex mutex_;
    std::optional<PerceptionObservation> latest_;
    std::vector<Detection> activeDetections_;
    std::vector<PerceptionEvent> events_;
    int latestEventId_{0};
    std::string lastInstanceId_;
    int lastObservedSequence_{-1};
};

}  // namespace patrol
