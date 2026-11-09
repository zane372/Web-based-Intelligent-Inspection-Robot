#include "perception/perception_service.hpp"

#include <algorithm>

namespace patrol {

PerceptionService::PerceptionService(PerceptionConfig config,
                                     std::string instanceId, Grid grid)
    : config_(std::move(config)),
      instanceId_(instanceId),
      grid_(std::move(grid)),
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

    // 去重：忽略同实例下更旧或重复的观察。
    if (sequence <= lastObservedSequence_) return;
    lastObservedSequence_ = sequence;

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
    if (lastResult == LastResult::BlockedBoundary) {
        appendEventLocked(PerceptionEventType::ContactBoundary, instanceId,
                          sequence, position, std::nullopt,
                          LastResult::BlockedBoundary);
    } else if (lastResult == LastResult::BlockedObstacle) {
        appendEventLocked(PerceptionEventType::ContactObstacle, instanceId,
                          sequence, position, std::nullopt,
                          LastResult::BlockedObstacle);
    }

    // 5) 更新活动检测与最新观察（原子发布）。
    activeDetections_ = detections;
    latest_ = PerceptionObservation{sequence, config_.rangeCells, ranges,
                                    detections};
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
    view.truncated = !events_.empty() && after + 1 < events_.front().eventId;

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
