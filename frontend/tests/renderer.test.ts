import { describe, expect, it } from "vitest";
import { cellCenter, DEFAULT_CELL_SIZE, draw } from "../src/canvas/renderer";
import type { MapInfo, RobotState } from "../src/types";

const map: MapInfo = {
  mapId: "grid-20x20",
  width: 20,
  height: 20,
  origin: "TOP_LEFT",
  xAxis: "RIGHT",
  yAxis: "DOWN",
};

const robot: RobotState = {
  robotId: "robot-1",
  instanceId: "i1",
  position: { x: 3, y: 4 },
  heading: "RIGHT",
  motionStatus: "IDLE",
  lastResult: "MOVED",
  sequence: 1,
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

describe("draw", () => {
  it("绘制地图与机器人不抛异常", () => {
    expect(() => draw(mockCtx(), map, robot, 32)).not.toThrow();
  });

  it("机器人不存在时仅绘制网格不抛异常", () => {
    expect(() => draw(mockCtx(), map, null, 32)).not.toThrow();
  });
});
