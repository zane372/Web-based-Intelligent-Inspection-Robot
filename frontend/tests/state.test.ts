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
    expect(s1.robot).toBeNull();
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

  it("EXECUTING 期间轮询只更新数据不改变状态", () => {
    const s0 = reduce(createInitialState(), { type: "loaded", map, robot: robot() });
    const s1 = reduce(s0, { type: "commandStart" });
    const s2 = reduce(s1, { type: "pollOk", robot: robot({ sequence: 1 }) });
    expect(s2.ui).toBe("EXECUTING");
    expect(s2.robot?.sequence).toBe(1);
  });
});
