// Canvas 2D 渲染器：工业安全现场的网格地图、警戒边界、障碍物（危险区）、
// 机器人本体与朝向指示、已发现异常图层。不依赖 DOM 格子；
// 坐标换算 pixelX = (x + 0.5) * cellSize。
import { severityColor } from "../format";
import type { Detection, Heading, MapInfo, RobotState } from "../types";

export const DEFAULT_CELL_SIZE = 32;

// 工业安全现场配色（浅色现场，适配明亮演示/投影环境）。
const FIELD = "#f2f5f7"; // 现场浅底
const GRID = "#d9e0e7"; // 网格细线
const BOUNDARY = "#1b2531"; // 外边界（工装墨色）
const BOUNDARY_WARN = "#d97706"; // 边界警示内线（安全琥珀）
const OBSTACLE = "#26303c"; // 障碍底色（深炭）
const OBSTACLE_SLASH = "#e2a50c"; // 障碍斜纹（警戒黄）
const ROBOT_BODY = "#1b2531"; // 机器人本体
const ROBOT_HEADING = "#e2a50c"; // 朝向指示（警戒黄）
const ROBOT_RING = "#d97706"; // 传感器环（安全琥珀）

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
    default:
      // 防御：非法 heading 已被 validate 拦截，理论不可达；返回零向量避免 drawRobot 崩溃。
      return { x: 0, y: 0 };
  }
}

function drawRobot(
  ctx: CanvasRenderingContext2D,
  robot: RobotState,
  cellSize: number,
): void {
  const center = cellCenter(robot.position, cellSize);
  const radius = cellSize * 0.4;

  // 传感器环：本体外围的薄环，体现巡检设备的传感范围。
  ctx.strokeStyle = ROBOT_RING;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(center.x, center.y, cellSize * 0.52, 0, Math.PI * 2);
  ctx.stroke();

  // 本体：填充圆（深炭，像巡检设备的工程外壳）。
  ctx.fillStyle = ROBOT_BODY;
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
  ctx.fillStyle = ROBOT_HEADING;
  ctx.beginPath();
  ctx.moveTo(tip.x, tip.y);
  ctx.lineTo(base1.x, base1.y);
  ctx.lineTo(base2.x, base2.y);
  ctx.closePath();
  ctx.fill();
}

function drawObstacle(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  cellSize: number,
): void {
  ctx.fillStyle = OBSTACLE;
  ctx.fillRect(x, y, cellSize, cellSize);
  // 危险斜纹：角到角的警戒黄斜线（始终在格内），图形化区分障碍（非仅颜色）。
  ctx.strokeStyle = OBSTACLE_SLASH;
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.moveTo(x + 2, y + cellSize - 2);
  ctx.lineTo(x + cellSize - 2, y + 2);
  ctx.stroke();
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
  ctx.fillStyle = FIELD;
  ctx.fillRect(0, 0, width, height);

  // 网格线（细）。
  ctx.strokeStyle = GRID;
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

  // 障碍物：深色危险区 + 警戒斜纹，机器人无法进入。
  for (const o of map.obstacles) {
    drawObstacle(ctx, o.x * cellSize, o.y * cellSize, cellSize);
  }

  // 外边界：双警戒线（墨色外框 + 安全琥珀警示内线）。
  ctx.strokeStyle = BOUNDARY;
  ctx.lineWidth = 3;
  ctx.strokeRect(1.5, 1.5, width - 3, height - 3);
  ctx.strokeStyle = BOUNDARY_WARN;
  ctx.lineWidth = 1.5;
  ctx.strokeRect(6, 6, width - 12, height - 12);

  if (robot) {
    drawRobot(ctx, robot, cellSize);
  }
}

// 感知图层：只绘制「已发现」的异常（警告三角 + 感叹号文字，非仅颜色）。
// 不得泄露未发现异常的位置（SPEC 6.8）。
export function drawPerceptionLayer(
  ctx: CanvasRenderingContext2D,
  discovered: Detection[],
  cellSize: number,
): void {
  for (const d of discovered) {
    const center = cellCenter(d.position, cellSize);
    const r = cellSize * 0.38;
    ctx.fillStyle = severityColor(d.severity);
    ctx.beginPath();
    ctx.moveTo(center.x, center.y - r);
    ctx.lineTo(center.x + r * 0.92, center.y + r * 0.72);
    ctx.lineTo(center.x - r * 0.92, center.y + r * 0.72);
    ctx.closePath();
    ctx.fill();
    ctx.fillStyle = "#ffffff";
    ctx.font = `bold ${Math.round(cellSize * 0.5)}px sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("!", center.x, center.y + r * 0.12);
  }
}
