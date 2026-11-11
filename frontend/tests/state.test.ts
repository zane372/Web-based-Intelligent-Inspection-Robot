import { describe, expect, it } from "vitest";
import { createInitialState, MAX_POLL_FAILURES, reduce } from "../src/state";
import type { MapInfo, RobotState } from "../src/types";

function robot(overrides: Partial<RobotState> = {}): RobotState {
  return {
    robotId: "robot-1",
    instanceId: "i1",
    position: { x: 0, y: 0 },
    heading: "RIGHT",
    motionStatus: "IDLE",
    lastResult: "NONE",
    sequence: 0,
    patrolActive: false,
    ...overrides,
  };
}

const map: MapInfo = {
  mapId: "grid-20x20",
  width: 20,
  height: 20,
  origin: "TOP_LEFT",
  xAxis: "RIGHT",
  yAxis: "DOWN",
  obstacles: [],
};

describe("state reducer", () => {
  it("初始为 LOADING", () => {
    expect(createInitialState().ui).toBe("LOADING");
  });

  it("loaded 后进入 READY 并持有地图与机器人", () => {
    const s = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    expect(s.ui).toBe("READY");
    expect(s.map?.mapId).toBe("grid-20x20");
    expect(s.robot?.robotId).toBe("robot-1");
  });

  it("忽略过期序列号（同实例、更小 sequence）", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ sequence: 5 }),
    });
    const s1 = reduce(s0, { type: "pollOk", robot: robot({ sequence: 3 }) });
    expect(s1.robot?.sequence).toBe(5);
  });

  it("接受更新的序列号", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ sequence: 5 }),
    });
    const s1 = reduce(s0, {
      type: "pollOk",
      robot: robot({ sequence: 6, position: { x: 1, y: 0 } }),
    });
    expect(s1.robot?.sequence).toBe(6);
    expect(s1.robot?.position).toEqual({ x: 1, y: 0 });
  });

  it("instanceId 变化进入 LOADING 并标记重载", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ instanceId: "i1" }),
    });
    const s1 = reduce(s0, { type: "pollOk", robot: robot({ instanceId: "i2" }) });
    expect(s1.ui).toBe("LOADING");
    expect(s1.instanceChanged).toBe(true);
    // 保留旧快照（不置 null），否则后续过期响应会跳过 instanceId/sequence 校验。
    expect(s1.robot?.instanceId).toBe("i1");
  });

  it("连续 3 次轮询失败进入 DISCONNECTED", () => {
    let s = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    s = reduce(s, { type: "pollFail" });
    s = reduce(s, { type: "pollFail" });
    expect(s.ui).toBe("DEGRADED");
    expect(s.pollFailures).toBe(2);
    s = reduce(s, { type: "pollFail" });
    expect(s.ui).toBe("DISCONNECTED");
    expect(s.pollFailures).toBe(MAX_POLL_FAILURES);
  });

  it("轮询成功清零失败计数", () => {
    let s = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    s = reduce(s, { type: "pollFail" });
    s = reduce(s, { type: "pollOk", robot: robot() });
    expect(s.ui).toBe("READY");
    expect(s.pollFailures).toBe(0);
  });

  it("命令执行：EXECUTING → BOUNDARY_FEEDBACK → READY", () => {
    let s = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    s = reduce(s, { type: "commandStart" });
    expect(s.ui).toBe("EXECUTING");
    s = reduce(s, {
      type: "commandDone",
      robot: robot({ sequence: 1, motionStatus: "BLOCKED", lastResult: "BLOCKED_BOUNDARY" }),
      result: "BLOCKED_BOUNDARY",
    });
    expect(s.ui).toBe("BOUNDARY_FEEDBACK");
    s = reduce(s, { type: "feedbackTimeout" });
    expect(s.ui).toBe("READY");
  });

  it("命令成功（MOVED/RESET）直接回到 READY", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, {
      type: "commandDone",
      robot: robot({ sequence: 1, position: { x: 1, y: 0 }, lastResult: "MOVED" }),
      result: "MOVED",
    });
    expect(s2.ui).toBe("READY");
    expect(s2.robot?.position).toEqual({ x: 1, y: 0 });
  });

  it("LOADING 期间 commandDone 被忽略（重载中迟到命令响应不覆盖状态）", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    // 服务重启：pollOk 检测到 instanceId 变化进入 LOADING。
    const s2 = reduce(s1, { type: "pollOk", robot: robot({ instanceId: "i2" }) });
    expect(s2.ui).toBe("LOADING");
    // 在途命令响应（旧实例 i1）不得把 LOADING 覆盖回 READY/BOUNDARY_FEEDBACK。
    const s3 = reduce(s2, {
      type: "commandDone",
      robot: robot({ instanceId: "i1", sequence: 1, lastResult: "MOVED" }),
      result: "MOVED",
    });
    expect(s3.ui).toBe("LOADING");
    expect(s3.robot?.sequence).toBe(0);
  });

  it("commandDone 来自旧实例被忽略（重载后迟到响应不覆盖新实例状态）", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ instanceId: "i2", sequence: 3 }),
    });
    // 迟到命令响应来自旧实例 i1 → 忽略，保持当前 i2 状态。
    const s1 = reduce(s0, {
      type: "commandDone",
      robot: robot({ instanceId: "i1", sequence: 5, lastResult: "MOVED" }),
      result: "MOVED",
    });
    expect(s1.ui).toBe("READY");
    expect(s1.robot?.instanceId).toBe("i2");
    expect(s1.robot?.sequence).toBe(3);
  });

  it("命令被障碍阻挡进入 BOUNDARY_FEEDBACK", () => {
    let s = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    s = reduce(s, { type: "commandStart" });
    s = reduce(s, {
      type: "commandDone",
      robot: robot({ sequence: 1, motionStatus: "BLOCKED", lastResult: "BLOCKED_OBSTACLE" }),
      result: "BLOCKED_OBSTACLE",
    });
    expect(s.ui).toBe("BOUNDARY_FEEDBACK");
    s = reduce(s, { type: "feedbackTimeout" });
    expect(s.ui).toBe("READY");
  });

  it("LOADING 期间 feedbackTimeout 不改变状态（服务重启竞态防护）", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    // 服务重启：pollOk 检测到 instanceId 变化进入 LOADING。
    const s1 = reduce(s0, {
      type: "pollOk",
      robot: robot({ instanceId: "i2", sequence: 0 }),
    });
    expect(s1.ui).toBe("LOADING");
    // 1.5s 反馈窗口内的 feedbackTimeout 不应把 LOADING 拉回 READY。
    expect(reduce(s1, { type: "feedbackTimeout" }).ui).toBe("LOADING");
  });

  it("EXECUTING 期间轮询只更新数据不改变状态", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, { type: "pollOk", robot: robot({ sequence: 1 }) });
    expect(s2.ui).toBe("EXECUTING");
    expect(s2.robot?.sequence).toBe(1);
  });

  it("BOUNDARY_FEEDBACK 期间轮询只更新数据不改变状态（反馈不被 1s 轮询覆盖）", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, {
      type: "commandDone",
      robot: robot({ sequence: 1, motionStatus: "BLOCKED", lastResult: "BLOCKED_BOUNDARY" }),
      result: "BLOCKED_BOUNDARY",
    });
    expect(s2.ui).toBe("BOUNDARY_FEEDBACK");
    const s3 = reduce(s2, {
      type: "pollOk",
      robot: robot({ sequence: 1, motionStatus: "BLOCKED", lastResult: "BLOCKED_BOUNDARY" }),
    });
    expect(s3.ui).toBe("BOUNDARY_FEEDBACK");
    expect(s3.robot?.sequence).toBe(1);
    // 反馈展示期结束由 feedbackTimeout 收尾到 READY。
    expect(reduce(s3, { type: "feedbackTimeout" }).ui).toBe("READY");
  });

  it("命令发送失败后 commandFailed 回到 READY（不再卡死 EXECUTING）", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    expect(s1.ui).toBe("EXECUTING");
    const s2 = reduce(s1, {
      type: "commandFailed",
      robot: robot({ sequence: 1, position: { x: 1, y: 0 }, lastResult: "MOVED" }),
    });
    expect(s2.ui).toBe("READY");
    expect(s2.robot?.position).toEqual({ x: 1, y: 0 });
    expect(s2.pollFailures).toBe(0);
  });

  it("命令失败且服务重启（instanceId 变化）时 commandFailed 进入 LOADING", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ instanceId: "i1" }),
    });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, { type: "commandFailed", robot: robot({ instanceId: "i2" }) });
    expect(s2.ui).toBe("LOADING");
    expect(s2.instanceChanged).toBe(true);
    // 保留旧快照，防过期响应打回 READY。
    expect(s2.robot?.instanceId).toBe("i1");
  });

  it("LOADING 期间收到旧实例的过期轮询不回退到 READY", () => {
    // 复现 M-3 竞态：服务重启后 commandFailed 先置 LOADING，随后在途的旧轮询
    // 返回旧实例数据。旧实现会因 robot 被置 null 而跳过校验、把 LOADING 打回
    // READY 并写入旧数据；修复后应保持 LOADING 直到 loadAll 完成。
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ instanceId: "i1", sequence: 5 }),
    });
    // 轮询发现服务重启：进入 LOADING（保留旧快照 i1）。
    const s1 = reduce(s0, { type: "pollOk", robot: robot({ instanceId: "i2" }) });
    expect(s1.ui).toBe("LOADING");
    // 在途的旧轮询返回旧实例（i1）数据：不得把 LOADING 打回 READY。
    const s2 = reduce(s1, {
      type: "pollOk",
      robot: robot({ instanceId: "i1", sequence: 6, position: { x: 3, y: 3 } }),
    });
    expect(s2.ui).toBe("LOADING");
    // 数据可更新（旧实例快照），但 UI 态仍是 LOADING。
    expect(s2.robot?.instanceId).toBe("i1");
  });

  it("commandFailed 即使收到更小序列号也回到 READY 并刷新快照（不做过期丢弃）", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ sequence: 5 }),
    });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, {
      type: "commandFailed",
      robot: robot({ sequence: 3, lastResult: "MOVED" }),
    });
    expect(s2.ui).toBe("READY");
    expect(s2.robot?.sequence).toBe(3);
  });

  it("命令失败时若巡检受阻仍正确设置报警锁存", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ patrolActive: true, sequence: 10 }),
    });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, {
      type: "commandFailed",
      robot: robot({
        patrolActive: true, sequence: 11,
        motionStatus: "BLOCKED", lastResult: "BLOCKED_OBSTACLE",
      }),
    });
    expect(s2.ui).toBe("READY");
    expect(s2.patrolAlert).toBe("BLOCKED_OBSTACLE");
  });

  it("巡检期间受阻（障碍物）设置报警锁存", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ patrolActive: true, sequence: 10 }),
    });
    const s1 = reduce(s0, {
      type: "pollOk",
      robot: robot({
        patrolActive: true, sequence: 11,
        motionStatus: "BLOCKED", lastResult: "BLOCKED_OBSTACLE",
      }),
    });
    expect(s1.patrolAlert).toBe("BLOCKED_OBSTACLE");
  });

  it("巡检期间受阻（边界）设置报警锁存", () => {
    const s0 = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ patrolActive: true, sequence: 10 }),
    });
    const s1 = reduce(s0, {
      type: "pollOk",
      robot: robot({
        patrolActive: true, sequence: 11,
        motionStatus: "BLOCKED", lastResult: "BLOCKED_BOUNDARY",
      }),
    });
    expect(s1.patrolAlert).toBe("BLOCKED_BOUNDARY");
  });

  it("报警锁存跨后续 MOVED 轮询保持，超时清除", () => {
    let s = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ patrolActive: true, sequence: 10 }),
    });
    s = reduce(s, {
      type: "pollOk",
      robot: robot({
        patrolActive: true, sequence: 11,
        motionStatus: "BLOCKED", lastResult: "BLOCKED_OBSTACLE",
      }),
    });
    expect(s.patrolAlert).toBe("BLOCKED_OBSTACLE");
    // 下一步 MOVED：锁存保持，不被覆盖。
    s = reduce(s, {
      type: "pollOk",
      robot: robot({ patrolActive: true, sequence: 12, lastResult: "MOVED" }),
    });
    expect(s.patrolAlert).toBe("BLOCKED_OBSTACLE");
    // 超时清除。
    s = reduce(s, { type: "patrolAlertTimeout" });
    expect(s.patrolAlert).toBeNull();
  });

  it("巡检结束清除报警锁存", () => {
    let s = reduce(createInitialState(), {
      type: "loaded", map, robot: robot({ patrolActive: true, sequence: 10 }),
    });
    s = reduce(s, {
      type: "pollOk",
      robot: robot({
        patrolActive: true, sequence: 11,
        motionStatus: "BLOCKED", lastResult: "BLOCKED_OBSTACLE",
      }),
    });
    expect(s.patrolAlert).toBe("BLOCKED_OBSTACLE");
    // patrolActive 变 false（巡检完成/停止）→ 锁存清除。
    s = reduce(s, {
      type: "pollOk",
      robot: robot({ patrolActive: false, sequence: 12, lastResult: "PATROL_COMPLETED" }),
    });
    expect(s.patrolAlert).toBeNull();
  });
});
