#include "perception/perception_service.hpp"

#include <algorithm>

namespace patrol {

PerceptionService::PerceptionService(PerceptionConfig config,
                                     std::string instanceId, Grid grid,
                                     std::uint32_t rngSeed)
    : config_(std::move(config)),
      instanceId_(instanceId),
      grid_(std::move(grid)),
      rng_(rngSeed == 0 ? std::random_device{}() : rngSeed),
      lastInstanceId_(std::move(instanceId)) {}

void PerceptionService::observe(const std::string& instanceId, int sequence,
                                Position position, Direction heading,
                                LastResult lastResult) {
    if (!config_.enabled) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // 新实例：重置全部感知状态（instanceId 变更 = 服务重启）。
    if (instanceId != lastInstanceId_) {
        latest_.reset();
        activeDetections_.clear();
        events_.clear();
        latestEventId_ = 0;
        instanceId_ = instanceId;
        lastInstanceId_ = instanceId;
        lastObservedSequence_ = -1;
    }

    // 去重：重复观察（==）直接忽略——幂等命令（已巡检的 START_PATROL、未巡检的
    // STOP_PATROL）会以同一 sequence 再次触发观察，若补记 CONTACT 会产生重复事件。
    if (sequence == lastObservedSequence_) {
        return;
    }
    // 乱序观察（<）：观察可能因「命令线程与巡检线程都在控制器锁外调用 observer」而
    // 乱序到达（高序列先取得感知锁）。此时不能更新 latest_ 与活动检测集——那会把
    // 快照回退、且异常进出事件会基于错误的 activeDetections_ 基线产生假事件；
    // 但接触事件（CONTACT）只依赖运动结果、不依赖活动集，是安全相关的边沿信号，
    // 乱序到达时仍须补记，否则并发命令会「偷跑」更高序列而丢巡检步的接触事件。
    if (sequence < lastObservedSequence_) {
        appendContactEventLocked(instanceId, sequence, position, lastResult);
        return;
    }
    lastObservedSequence_ = sequence;

    // RESET：重新随机化异常源位置（修复位置固定不变的缺陷），
    // 在本次观察计算前应用，使 reset 快照立即反映新位置。
    if (lastResult == LastResult::Reset) {
        randomizeHazardsLocked();
    }

    // 1) 三向测距。
    const std::vector<RangeReading> ranges =
        computeRangeReadings(grid_, position, heading, config_.rangeCells);

    // 2) 异常检测。
    const std::vector<Detection> detections =
        detectHazards(config_.hazards, position, config_.hazardRangeCells);

    // 3) 活动差异 → 事件。先产生新进入，再产生离开。
    for (const Detection& d : detections) {
        const bool wasActive = std::any_of(
            activeDetections_.begin(), activeDetections_.end(),
            [&](const Detection& a) { return a.hazardId == d.hazardId; });
        if (!wasActive) {
            appendEventLocked(PerceptionEventType::AnomalyDetected, instanceId,
                              sequence, position, d, std::nullopt);
        }
    }
    for (const Detection& d : activeDetections_) {
        const bool stillActive = std::any_of(
            detections.begin(), detections.end(),
            [&](const Detection& a) { return a.hazardId == d.hazardId; });
        if (!stillActive) {
            appendEventLocked(PerceptionEventType::AnomalyOutOfRange,
                              instanceId, sequence, position, d, std::nullopt);
        }
    }

    // 4) 接触事件（lastResult 首次变为受阻态）。
    appendContactEventLocked(instanceId, sequence, position, lastResult);

    // 5) 更新活动检测与最新观察（原子发布）。
    activeDetections_ = detections;
    latest_ = PerceptionObservation{sequence, config_.rangeCells, ranges,
                                    detections};
}

std::vector<Position> PerceptionService::hazardPositions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Position> out;
    out.reserve(config_.hazards.size());
    for (const Hazard& h : config_.hazards) {
        out.push_back(h.position);
    }
    return out;
}

void PerceptionService::randomizeHazardsLocked() {
    const auto positions = sampleHazardPositions(config_.hazards, grid_, rng_);
    if (positions.size() != config_.hazards.size()) {
        return;  // 可通行格不足（真值配置下不会发生），保持原位。
    }
    for (std::size_t i = 0; i < config_.hazards.size(); ++i) {
        config_.hazards[i].position = positions[i];
    }
}

PerceptionLatestView PerceptionService::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    PerceptionLatestView view;
    view.enabled = config_.enabled;
    view.instanceId = instanceId_;
    view.observation = latest_;
    view.activeDetections = activeDetections_;
    view.latestEventId = latestEventId_;
    return view;
}

PerceptionEventsView PerceptionService::events(int after, int limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    PerceptionEventsView view;
    view.instanceId = instanceId_;
    view.latestEventId = latestEventId_;
    view.oldestEventId = events_.empty() ? 0 : events_.front().eventId;
    // 调用方游标 after 之后到最旧保留事件之间存在被环形缓冲丢弃的事件
    // 时标记 truncated（SPEC 6.7.2：客户端须重取 latest 再从新基线续拉）。
    // 用 `after < front.eventId - 1` 而非 `after + 1 < front.eventId`：
    // eventId 从 1 起，`front.eventId - 1` 不会下溢；而 after==INT_MAX 时
    // `after + 1` 会触发有符号整数溢出（未定义行为）。
    view.truncated = !events_.empty() && after < events_.front().eventId - 1;

    int returned = 0;
    for (const PerceptionEvent& e : events_) {
        if (e.eventId <= after) continue;
        if (returned >= limit) {
            view.hasMore = true;
            break;
        }
        view.events.push_back(e);
        ++returned;
    }
    return view;
}

void PerceptionService::appendContactEventLocked(
    const std::string& instanceId, int sequence, Position position,
    LastResult lastResult) {
    if (lastResult == LastResult::BlockedBoundary) {
        appendEventLocked(PerceptionEventType::ContactBoundary, instanceId,
                          sequence, position, std::nullopt,
                          LastResult::BlockedBoundary);
    } else if (lastResult == LastResult::BlockedObstacle) {
        appendEventLocked(PerceptionEventType::ContactObstacle, instanceId,
                          sequence, position, std::nullopt,
                          LastResult::BlockedObstacle);
    }
}

void PerceptionService::appendEventLocked(
    PerceptionEventType type, const std::string& instanceId, int sequence,
    Position position, const std::optional<Detection>& hazard,
    const std::optional<LastResult>& contact) {
    PerceptionEvent event;
    event.eventId = ++latestEventId_;
    event.instanceId = instanceId;
    event.sequence = sequence;
    event.type = type;
    event.position = position;
    event.hazard = hazard;
    event.contactResult = contact;
    events_.push_back(std::move(event));

    if (events_.size() > kEventBufferCapacity) {
        events_.erase(events_.begin(),
                      events_.begin() +
                          static_cast<std::ptrdiff_t>(
                              events_.size() - kEventBufferCapacity));
    }
}

}  // namespace patrol
