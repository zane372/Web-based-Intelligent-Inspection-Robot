#include <gtest/gtest.h>

#include <random>
#include <set>

#include "domain/grid.hpp"
#include "perception/perception_rules.hpp"
#include "perception/perception_service.hpp"

namespace patrol {
namespace {

using ::patrol::Direction;
using ::patrol::Position;

// ---------- 纯规则：三向旋转与测距（VP-01 ~ VP-05） ----------

TEST(PerceptionRules, RotateMappingIsCorrect) {
    // VP-04：四种朝向下前/左/右绝对方向映射正确。
    EXPECT_EQ(rotateLeft(Direction::Up), Direction::Left);
    EXPECT_EQ(rotateRight(Direction::Up), Direction::Right);
    EXPECT_EQ(rotateLeft(Direction::Down), Direction::Right);
    EXPECT_EQ(rotateRight(Direction::Down), Direction::Left);
    EXPECT_EQ(rotateLeft(Direction::Left), Direction::Down);
    EXPECT_EQ(rotateRight(Direction::Left), Direction::Up);
    EXPECT_EQ(rotateLeft(Direction::Right), Direction::Up);
    EXPECT_EQ(rotateRight(Direction::Right), Direction::Down);
}

TEST(PerceptionRules, CastRayLeftBoundary) {
    // VP-01：贴左边界（x=0），朝左的射线 1 格处命中边界，cell 为 null。
    Grid grid(5, 5);
    const Position pos{0, 2};
    const RangeReading r =
        castRay(grid, pos, Direction::Left, 3, RelativeDirection::Left,
                "range-left");
    EXPECT_EQ(r.hitType, HitType::Boundary);
    EXPECT_EQ(r.distanceCells, 1);
    EXPECT_FALSE(r.cell.has_value());
}

TEST(PerceptionRules, CastRayNearestObstacleNoPenetration) {
    // VP-02：前方 2 格为障碍，其后还有障碍——只返回最近障碍、距离 2、不穿透。
    Grid grid(6, 3, {Position{3, 1}, Position{4, 1}});
    const RangeReading r =
        castRay(grid, Position{1, 1}, Direction::Right, 5,
                RelativeDirection::Front, "range-front");
    EXPECT_EQ(r.hitType, HitType::Obstacle);
    EXPECT_EQ(r.distanceCells, 2);
    ASSERT_TRUE(r.cell.has_value());
    EXPECT_EQ(r.cell->x, 3);
    EXPECT_EQ(r.cell->y, 1);
}

TEST(PerceptionRules, CastRayNoHitInRange) {
    // VP-03：量程内无边界/障碍 → NONE，distance 与 cell 均无意义。
    Grid grid(20, 20);
    const RangeReading r =
        castRay(grid, Position{5, 5}, Direction::Right, 3,
                RelativeDirection::Front, "range-front");
    EXPECT_EQ(r.hitType, HitType::None);
    EXPECT_FALSE(r.cell.has_value());
}

TEST(PerceptionRules, ThreeWayReadingsFollowHeading) {
    // VP-04 补充：heading=RIGHT 时 front=RIGHT、left=UP、right=DOWN。
    Grid grid(10, 10);
    const auto readings =
        computeRangeReadings(grid, Position{3, 3}, Direction::Right, 5);
    ASSERT_EQ(readings.size(), 3u);
    EXPECT_EQ(readings[0].sensorId, "range-front");
    EXPECT_EQ(readings[0].relativeDirection, RelativeDirection::Front);
    EXPECT_EQ(readings[0].absoluteDirection, Direction::Right);
    EXPECT_EQ(readings[1].sensorId, "range-left");
    EXPECT_EQ(readings[1].relativeDirection, RelativeDirection::Left);
    EXPECT_EQ(readings[1].absoluteDirection, Direction::Up);
    EXPECT_EQ(readings[2].sensorId, "range-right");
    EXPECT_EQ(readings[2].relativeDirection, RelativeDirection::Right);
    EXPECT_EQ(readings[2].absoluteDirection, Direction::Down);
}

TEST(PerceptionRules, DetectHazardManhattanThreshold) {
    // VP-05：曼哈顿距离等于半径时检测，超出 1 格不检测。
    std::vector<Hazard> hazards;
    Hazard h;
    h.id = "h1";
    h.type = HazardType::Smoke;
    h.severity = Severity::High;
    h.position = Position{3, 0};
    hazards.push_back(h);

    const int radius = 2;
    // 距离 == radius：检测。
    EXPECT_EQ(detectHazards(hazards, Position{1, 0}, radius).size(), 1u);
    // 距离 == radius + 1：不检测。
    EXPECT_EQ(detectHazards(hazards, Position{0, 0}, radius).size(), 0u);
}

TEST(PerceptionRules, DetectHazardsSortedById) {
    std::vector<Hazard> hazards;
    for (const auto& id : {"b", "a", "c"}) {
        Hazard h;
        h.id = id;
        h.type = HazardType::GasLeak;
        h.position = Position{5, 5};
        hazards.push_back(h);
    }
    const auto detections = detectHazards(hazards, Position{5, 5}, 0);
    ASSERT_EQ(detections.size(), 3u);
    EXPECT_EQ(detections[0].hazardId, "a");
    EXPECT_EQ(detections[1].hazardId, "b");
    EXPECT_EQ(detections[2].hazardId, "c");
}

// ---------- 服务：异常/接触事件、去重、环形缓冲（VP-06 ~ VP-13） ----------

PerceptionConfig makeConfig(int rangeCells, int hazardRangeCells,
                            std::vector<Hazard> hazards, bool enabled = true) {
    PerceptionConfig c;
    c.enabled = enabled;
    c.rangeCells = rangeCells;
    c.hazardRangeCells = hazardRangeCells;
    c.hazards = std::move(hazards);
    return c;
}

Hazard makeHazard(std::string id, Position pos) {
    Hazard h;
    h.id = std::move(id);
    h.type = HazardType::Smoke;
    h.severity = Severity::High;
    h.position = pos;
    h.label = "烟雾";
    return h;
}

TEST(PerceptionService, ContinuousDetectionSingleEvent) {
    // VP-06：同一异常连续停留多个序列号，只产生一次 DETECTED，活动读数持续更新。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 0, Position{0, 0}, Direction::Right,
                LastResult::None);  // 距离 5 > 2
    svc.observe("inst-1", 1, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // 距离 2 → DETECTED
    svc.observe("inst-1", 2, Position{4, 0}, Direction::Right,
                LastResult::Moved);  // 距离 1 → 仍在半径内，无新事件

    const auto view = svc.latest();
    EXPECT_EQ(view.latestEventId, 1);
    ASSERT_EQ(view.activeDetections.size(), 1u);
    EXPECT_EQ(view.activeDetections[0].hazardId, "h1");
    EXPECT_EQ(view.activeDetections[0].distanceCells, 1);
    EXPECT_TRUE(view.observation.has_value());
    EXPECT_EQ(view.observation->sequence, 2);
}

TEST(PerceptionService, LeaveAndReenterGeneratesNewEvents) {
    // VP-07：离开半径、再次进入 → 各一次 OUT_OF_RANGE 与新的 DETECTED。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // DETECTED (event 1)
    svc.observe("inst-1", 1, Position{8, 0}, Direction::Right,
                LastResult::Moved);  // 距离 3 > 2 → OUT_OF_RANGE (event 2)
    svc.observe("inst-1", 2, Position{4, 0}, Direction::Right,
                LastResult::Moved);  // 距离 1 → DETECTED (event 3)

    const auto ev = svc.events(0, 50);
    ASSERT_EQ(ev.events.size(), 3u);
    EXPECT_EQ(ev.events[0].type, PerceptionEventType::AnomalyDetected);
    EXPECT_EQ(ev.events[1].type, PerceptionEventType::AnomalyOutOfRange);
    EXPECT_EQ(ev.events[2].type, PerceptionEventType::AnomalyDetected);
    // OUT_OF_RANGE 保留最后已知位置（不删除）。
    ASSERT_TRUE(ev.events[1].hazard.has_value());
    EXPECT_EQ(ev.events[1].hazard->position.x, 5);
    EXPECT_EQ(ev.events[1].hazard->position.y, 0);
}

TEST(PerceptionService, DedupIgnoresRepeatedAndOlder) {
    // VP-08：同一快照两次、旧快照后到 → 不重复事件、不覆盖新观察。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // DETECTED (event 1)
    svc.observe("inst-1", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // 重复，忽略
    svc.observe("inst-1", 1, Position{8, 0}, Direction::Right,
                LastResult::Moved);  // OUT_OF_RANGE (event 2)
    svc.observe("inst-1", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // 旧快照，忽略

    const auto view = svc.latest();
    EXPECT_EQ(view.latestEventId, 2);
    EXPECT_TRUE(view.observation.has_value());
    EXPECT_EQ(view.observation->sequence, 1);  // 未被旧快照覆盖
}

TEST(PerceptionService, ContactEvents) {
    // VP-09：边界/障碍被阻止 → 对应 CONTACT 事件恰好一条。
    auto config = makeConfig(3, 2, {});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 1, Position{0, 0}, Direction::Right,
                LastResult::BlockedBoundary);
    svc.observe("inst-1", 2, Position{4, 4}, Direction::Right,
                LastResult::BlockedObstacle);

    const auto ev = svc.events(0, 50);
    ASSERT_EQ(ev.events.size(), 2u);
    EXPECT_EQ(ev.events[0].type, PerceptionEventType::ContactBoundary);
    ASSERT_TRUE(ev.events[0].contactResult.has_value());
    EXPECT_EQ(*ev.events[0].contactResult, LastResult::BlockedBoundary);
    EXPECT_FALSE(ev.events[0].hazard.has_value());

    EXPECT_EQ(ev.events[1].type, PerceptionEventType::ContactObstacle);
    ASSERT_TRUE(ev.events[1].contactResult.has_value());
    EXPECT_EQ(*ev.events[1].contactResult, LastResult::BlockedObstacle);
}

TEST(PerceptionService, DisabledConfigNoOp) {
    // VP-12（未启用部分）：enabled=false → 观察不生效，返回空功能视图。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})},
                             /*enabled=*/false);
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 0, Position{5, 0}, Direction::Right,
                LastResult::None);

    const auto view = svc.latest();
    EXPECT_FALSE(view.enabled);
    EXPECT_FALSE(view.observation.has_value());
    EXPECT_TRUE(view.activeDetections.empty());
    EXPECT_EQ(view.latestEventId, 0);

    const auto ev = svc.events(0, 50);
    EXPECT_TRUE(ev.events.empty());
    EXPECT_EQ(ev.oldestEventId, 0);
    EXPECT_EQ(ev.latestEventId, 0);
    EXPECT_FALSE(ev.hasMore);
    EXPECT_FALSE(ev.truncated);
}

TEST(PerceptionService, RingBufferBoundedAndPaginated) {
    // VP-11：写入超过 200 条事件并分页 → 容量有界、oldest/hasMore/truncated/after 正确。
    // hazardRangeCells=0：只在 (0,0) 检测，交替位置产生 DETECTED/OUT_OF_RANGE 各一事件。
    auto config = makeConfig(3, 0, {makeHazard("h1", Position{0, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    const int kTotal = 405;  // 奇数，最后停在 DETECTED
    for (int i = 0; i < kTotal; ++i) {
        const bool atHazard = (i % 2 == 0);
        svc.observe("inst-1", i, atHazard ? Position{0, 0} : Position{1, 0},
                    Direction::Right, LastResult::Moved);
    }
    // 每个 i 都生成 1 个事件（0→DETECTED, 1→OUT_OF_RANGE, 2→DETECTED, ...）。
    const auto view = svc.latest();
    EXPECT_EQ(view.latestEventId, kTotal);

    // 缓冲只保留最近 200 条：oldest = kTotal - 200 + 1 = 206。
    const auto page1 = svc.events(0, 50);
    ASSERT_EQ(page1.events.size(), 50u);
    EXPECT_EQ(page1.events.front().eventId, 206);
    EXPECT_EQ(page1.oldestEventId, 206);
    EXPECT_EQ(page1.latestEventId, kTotal);
    EXPECT_TRUE(page1.hasMore);
    EXPECT_TRUE(page1.truncated);  // after=0 早于最旧保留事件

    // after 追到当前最新 → 空结果、无 hasMore、无 truncated。
    const auto last = svc.events(kTotal, 50);
    EXPECT_TRUE(last.events.empty());
    EXPECT_FALSE(last.hasMore);
    EXPECT_FALSE(last.truncated);

    // after 介于最旧与最新之间 → 正常增量、无截断。
    const auto mid = svc.events(300, 50);
    ASSERT_EQ(mid.events.size(), 50u);
    EXPECT_EQ(mid.events.front().eventId, 301);
    EXPECT_FALSE(mid.truncated);
}

TEST(PerceptionService, InstanceChangeResetsState) {
    // VP-13（服务重启部分）：instanceId 变更 → 事件编号与缓冲重置。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10));

    svc.observe("inst-1", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // event 1
    svc.observe("inst-2", 0, Position{3, 0}, Direction::Right,
                LastResult::Moved);  // 新实例 → event 1（重置）

    const auto view = svc.latest();
    EXPECT_EQ(view.instanceId, "inst-2");
    EXPECT_EQ(view.latestEventId, 1);
    const auto ev = svc.events(0, 50);
    ASSERT_EQ(ev.events.size(), 1u);
    EXPECT_EQ(ev.events[0].eventId, 1);
    EXPECT_EQ(ev.events[0].instanceId, "inst-2");
}

// ---------- RESET 随机化异常源位置（新增） ----------

TEST(PerceptionRules, SampleHazardPositionsWalkableAndDistinct) {
    // 随机化真值：抽取的位置均在界内可通行、互不重复、数量与异常源一致。
    Grid grid(5, 5, {Position{2, 2}});  // 1 个障碍 → 24 个可通行格
    std::vector<Hazard> hazards;
    for (const char* id : {"a", "b", "c"}) {
        hazards.push_back(makeHazard(id, Position{0, 0}));
    }
    std::mt19937 rng(20241110u);
    const auto positions = sampleHazardPositions(hazards, grid, rng);
    ASSERT_EQ(positions.size(), hazards.size());
    const std::set<Position> unique(positions.begin(), positions.end());
    EXPECT_EQ(unique.size(), positions.size());  // 互不重复
    for (const Position& p : positions) {
        EXPECT_TRUE(grid.isWalkable(p));
        EXPECT_FALSE(grid.isObstacle(p));
    }
}

TEST(PerceptionRules, SampleHazardPositionsEmptyWhenFewerWalkable) {
    // 可通行格少于异常源数量时无法保证互不重复，返回空向量由调用方保持原位。
    Grid grid(2, 1);  // 仅 (0,0)、(1,0) 可通行
    std::vector<Hazard> hazards;
    for (const char* id : {"a", "b", "c"}) {
        hazards.push_back(makeHazard(id, Position{0, 0}));
    }
    std::mt19937 rng(7u);
    const auto positions = sampleHazardPositions(hazards, grid, rng);
    EXPECT_TRUE(positions.empty());
}

TEST(PerceptionService, ResetMovesHazardToWalkableCell) {
    // 绕过配置校验直接把异常源放在障碍格，确定性证明 RESET 后位置被重新
    // 随机到可通行格（即位置确实改变，且始终落在可通行格内）。
    Grid grid(5, 5, {Position{2, 2}});
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{2, 2})});
    PerceptionService svc(config, "inst-1", grid, /*rngSeed=*/12345u);

    ASSERT_EQ(svc.hazardPositions().size(), 1u);
    EXPECT_EQ(svc.hazardPositions()[0], Position(2, 2));  // 初始位于障碍格

    svc.observe("inst-1", 1, Position{0, 0}, Direction::Right,
                LastResult::Reset);

    const auto positions = svc.hazardPositions();
    ASSERT_EQ(positions.size(), 1u);
    EXPECT_TRUE(grid.isWalkable(positions[0]));
    EXPECT_FALSE(grid.isObstacle(positions[0]));
}

TEST(PerceptionService, NonResetKeepsHazardPosition) {
    // MOVE/None 不触发随机化：异常源位置保持配置值不变。
    auto config = makeConfig(3, 2, {makeHazard("h1", Position{5, 0})});
    PerceptionService svc(config, "inst-1", Grid(10, 10), /*rngSeed=*/12345u);

    svc.observe("inst-1", 0, Position{0, 0}, Direction::Right,
                LastResult::None);
    svc.observe("inst-1", 1, Position{1, 0}, Direction::Right,
                LastResult::Moved);

    ASSERT_EQ(svc.hazardPositions().size(), 1u);
    EXPECT_EQ(svc.hazardPositions()[0], Position(5, 0));
}

TEST(PerceptionService, ResetRandomizesMultipleHazardsDistinct) {
    // 多个异常源 RESET 后互不重复且均在界内可通行。
    Grid grid(10, 10);
    auto config = makeConfig(
        3, 2,
        {makeHazard("a", Position{1, 1}), makeHazard("b", Position{8, 8}),
         makeHazard("c", Position{1, 8})});
    PerceptionService svc(config, "inst-1", grid, /*rngSeed=*/999u);

    svc.observe("inst-1", 1, Position{0, 0}, Direction::Right,
                LastResult::Reset);

    const auto positions = svc.hazardPositions();
    ASSERT_EQ(positions.size(), 3u);
    const std::set<Position> unique(positions.begin(), positions.end());
    EXPECT_EQ(unique.size(), 3u);  // 互不重复
    for (const Position& p : positions) {
        EXPECT_TRUE(grid.isWalkable(p));
    }
}

}  // namespace
}  // namespace patrol
