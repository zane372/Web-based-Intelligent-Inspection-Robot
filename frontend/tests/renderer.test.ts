import { describe, expect, it } from "vitest";
import {
  blockedCell,
  cellCenter,
  DEFAULT_CELL_SIZE,
  draw,
  drawPerceptionLayer,
  type BlockedFeedback,
} from "../src/canvas/renderer";
import type { Detection, MapInfo, RobotState } from "../src/types";

const map: MapInfo = {
  mapId: "grid-20x20",
  width: 20,
  height: 20,
  origin: "TOP_LEFT",
  xAxis: "RIGHT",
  yAxis: "DOWN",
  obstacles: [],
};

const robot: RobotState = {
  robotId: "robot-1",
  instanceId: "i1",
  position: { x: 3, y: 4 },
  heading: "RIGHT",
  motionStatus: "IDLE",
  lastResult: "MOVED",
  sequence: 1,
  patrolActive: false,
};

function mockCtx(): CanvasRenderingContext2D {
  const noop = (): void => {};
  const ctx = {
    clearRect: noop,
    fillRect: noop,
    strokeRect: noop,
    beginPath: noop,
    moveTo: noop,
    lineTo: noop,
    closePath: noop,
    stroke: noop,
    fill: noop,
    arc: noop,
    fillText: noop,
    setTransform: noop,
  } as unknown as CanvasRenderingContext2D;
  return ctx;
}

describe("cellCenter", () => {
  it("按 (x + 0.5) * cellSize 换算像素中心", () => {
    expect(cellCenter({ x: 0, y: 0 }, 32)).toEqual({ x: 16, y: 16 });
    expect(cellCenter({ x: 19, y: 19 }, 32)).toEqual({ x: 624, y: 624 });
    expect(cellCenter({ x: 1, y: 2 }, DEFAULT_CELL_SIZE)).toEqual({
      x: 48,
      y: 80,
    });
  });
});

describe("blockedCell", () => {
  it("朝向下一格在界内时返回该格", () => {
    const r: RobotState = { ...robot, position: { x: 3, y: 4 }, heading: "RIGHT" };
    expect(blockedCell(r, map)).toEqual({ x: 4, y: 4 });
  });

  it("越界时钳制到边缘格（真实反映碰撞发生在哪一侧）", () => {
    const up: RobotState = { ...robot, position: { x: 3, y: 0 }, heading: "UP" };
    expect(blockedCell(up, map)).toEqual({ x: 3, y: 0 });

    const left: RobotState = { ...robot, position: { x: 0, y: 4 }, heading: "LEFT" };
    expect(blockedCell(left, map)).toEqual({ x: 0, y: 4 });

    const right: RobotState = { ...robot, position: { x: 19, y: 4 }, heading: "RIGHT" };
    expect(blockedCell(right, map)).toEqual({ x: 19, y: 4 });

    const down: RobotState = { ...robot, position: { x: 3, y: 19 }, heading: "DOWN" };
    expect(blockedCell(down, map)).toEqual({ x: 3, y: 19 });
  });
});

describe("draw", () => {
  it("绘制地图与机器人不抛异常", () => {
    expect(() => draw(mockCtx(), map, robot, 32)).not.toThrow();
  });

  it("机器人不存在时仅绘制网格不抛异常", () => {
    expect(() => draw(mockCtx(), map, null, 32)).not.toThrow();
  });

  it("含障碍物时绘制不抛异常", () => {
    const obstacleMap: MapInfo = {
      ...map,
      obstacles: [{ x: 4, y: 4 }, { x: 5, y: 4 }],
    };
    expect(() => draw(mockCtx(), obstacleMap, robot, 32)).not.toThrow();
  });

  it("受阻反馈（红环扩散）绘制不抛异常", () => {
    const blocked: BlockedFeedback = { cell: { x: 4, y: 4 }, at: 0 };
    expect(() => draw(mockCtx(), map, robot, 32, [], 100, blocked)).not.toThrow();
  });

  it("减少动态模式下受阻反馈静态化绘制不抛异常", () => {
    const blocked: BlockedFeedback = { cell: { x: 4, y: 4 }, at: 0 };
    expect(() =>
      draw(mockCtx(), map, robot, 32, [], 100, blocked, true),
    ).not.toThrow();
  });
});

describe("drawPerceptionLayer", () => {
  it("绘制已发现异常不抛异常", () => {
    const discovered: Detection[] = [
      {
        hazardId: "h1",
        hazardType: "SMOKE",
        severity: "HIGH",
        label: null,
        position: { x: 3, y: 3 },
        distanceCells: 1,
      },
    ];
    expect(() => drawPerceptionLayer(mockCtx(), discovered, 32)).not.toThrow();
  });

  it("无异常时绘制不抛异常", () => {
    expect(() => drawPerceptionLayer(mockCtx(), [], 32)).not.toThrow();
  });
});
