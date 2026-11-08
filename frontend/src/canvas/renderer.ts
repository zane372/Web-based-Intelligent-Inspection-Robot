// Canvas 2D 渲染器：地图网格、外边界、机器人本体与朝向指示。
// 不依赖 DOM 格子；坐标换算 pixelX = (x + 0.5) * cellSize。
import type { Heading, MapInfo, RobotState } from "../types";

export const DEFAULT_CELL_SIZE = 32;

// 逻辑单元格中心对应的像素坐标。
export function cellCenter(
  pos: { x: number; y: number },
  cellSize: number,
): { x: number; y: number } {
  return { x: (pos.x + 0.5) * cellSize, y: (pos.y + 0.5) * cellSize };
}

// 初始化画布并处理高 DPI：backing store 按 devicePixelRatio 放大，
// 逻辑坐标仍以 cellSize 为单位的 CSS 像素。显示尺寸由 CSS 控制。
export function setupCanvas(
  canvas: HTMLCanvasElement,
  map: MapInfo,
  cellSize: number,
): CanvasRenderingContext2D | null {
  const dpr = window.devicePixelRatio || 1;
  const cssWidth = map.width * cellSize;
  const cssHeight = map.height * cellSize;
  canvas.width = Math.round(cssWidth * dpr);
  canvas.height = Math.round(cssHeight * dpr);
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    return null;
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return ctx;
}

function headingVector(heading: Heading): { x: number; y: number } {
  switch (heading) {
    case "UP":
      return { x: 0, y: -1 };
    case "DOWN":
      return { x: 0, y: 1 };
    case "LEFT":
      return { x: -1, y: 0 };
    case "RIGHT":
      return { x: 1, y: 0 };
  }
}

function drawRobot(
  ctx: CanvasRenderingContext2D,
  robot: RobotState,
  cellSize: number,
): void {
  const center = cellCenter(robot.position, cellSize);
  const radius = cellSize * 0.4;

  // 本体：填充圆。
  ctx.fillStyle = "#2563eb";
  ctx.beginPath();
  ctx.arc(center.x, center.y, radius, 0, Math.PI * 2);
  ctx.fill();

  // 朝向指示：朝 heading 方向伸出的实心三角（非仅靠颜色区分）。
  const dir = headingVector(robot.heading);
  const tip = {
    x: center.x + dir.x * (radius + cellSize * 0.25),
    y: center.y + dir.y * (radius + cellSize * 0.25),
  };
  const perp = { x: -dir.y, y: dir.x };
  const baseInset = radius * 0.3;
  const halfWidth = cellSize * 0.18;
  const base1 = {
    x: center.x + dir.x * baseInset + perp.x * halfWidth,
    y: center.y + dir.y * baseInset + perp.y * halfWidth,
  };
  const base2 = {
    x: center.x + dir.x * baseInset - perp.x * halfWidth,
    y: center.y + dir.y * baseInset - perp.y * halfWidth,
  };
  ctx.fillStyle = "#0f172a";
  ctx.beginPath();
  ctx.moveTo(tip.x, tip.y);
  ctx.lineTo(base1.x, base1.y);
  ctx.lineTo(base2.x, base2.y);
  ctx.closePath();
  ctx.fill();
}

export function draw(
  ctx: CanvasRenderingContext2D,
  map: MapInfo,
  robot: RobotState | null,
  cellSize: number,
): void {
  const width = map.width * cellSize;
  const height = map.height * cellSize;

  ctx.clearRect(0, 0, width, height);

  // 背景。
  ctx.fillStyle = "#f8fafc";
  ctx.fillRect(0, 0, width, height);

  // 网格线（细）。
  ctx.strokeStyle = "#e2e8f0";
  ctx.lineWidth = 1;
  for (let x = 1; x < map.width; x++) {
    ctx.beginPath();
    ctx.moveTo(x * cellSize, 0);
    ctx.lineTo(x * cellSize, height);
    ctx.stroke();
  }
  for (let y = 1; y < map.height; y++) {
    ctx.beginPath();
    ctx.moveTo(0, y * cellSize);
    ctx.lineTo(width, y * cellSize);
    ctx.stroke();
  }

  // 外边界（比网格线更粗）。
  ctx.strokeStyle = "#334155";
  ctx.lineWidth = 3;
  ctx.strokeRect(1.5, 1.5, width - 3, height - 3);

  if (robot) {
    drawRobot(ctx, robot, cellSize);
  }
}
