// Canvas 2D 渲染器：定向越野地图——可通行地形、等高线网格、密林障碍、
// 深土边界、紫色航线（已走轨迹）、紫色控制点机器人、指北针；感知异常图层。
// 不依赖 DOM 格子；坐标换算 pixelX = (x + 0.5) * cellSize。
import { severityColor } from "../format";
import { palette } from "../theme";
import type { Detection, Heading, MapInfo, Position, RobotState } from "../types";

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

  // 控制点环：紫色圆环（定向越野控制点符号）。
  ctx.strokeStyle = palette.course;
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  ctx.arc(center.x, center.y, cellSize * 0.48, 0, Math.PI * 2);
  ctx.stroke();

  // 本体：填充圆（墨色）。
  ctx.fillStyle = palette.robotBody;
  ctx.beginPath();
  ctx.arc(center.x, center.y, cellSize * 0.3, 0, Math.PI * 2);
  ctx.fill();

  // 朝向指示：朝 heading 方向伸出的紫色实心三角（非仅靠颜色区分）。
  const dir = headingVector(robot.heading);
  const tip = {
    x: center.x + dir.x * cellSize * 0.52,
    y: center.y + dir.y * cellSize * 0.52,
  };
  const perp = { x: -dir.y, y: dir.x };
  const halfWidth = cellSize * 0.16;
  const baseInset = cellSize * 0.08;
  const base1 = {
    x: center.x + dir.x * baseInset + perp.x * halfWidth,
    y: center.y + dir.y * baseInset + perp.y * halfWidth,
  };
  const base2 = {
    x: center.x + dir.x * baseInset - perp.x * halfWidth,
    y: center.y + dir.y * baseInset - perp.y * halfWidth,
  };
  ctx.fillStyle = palette.course;
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
  ctx.fillStyle = palette.obstacle;
  ctx.fillRect(x, y, cellSize, cellSize);
  // 密林纹理：两条斜线，图形化区分障碍（非仅颜色）。
  ctx.strokeStyle = palette.obstacleHatch;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(x + 3, y + cellSize - 3);
  ctx.lineTo(x + cellSize - 3, y + 3);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(x + cellSize / 2, y + cellSize - 2);
  ctx.lineTo(x + cellSize - 2, y + cellSize / 2);
  ctx.stroke();
}

// 指北针：右上角，指向上方（北 = 上 = -y）。
function drawNorthArrow(
  ctx: CanvasRenderingContext2D,
  width: number,
  cellSize: number,
): void {
  const cx = width - cellSize * 1.1;
  const cy = cellSize * 1.1;
  const r = cellSize * 0.45;

  // 箭杆。
  ctx.strokeStyle = palette.north;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(cx, cy + r);
  ctx.lineTo(cx, cy - r);
  ctx.stroke();

  // 箭头（向上）。
  ctx.fillStyle = palette.north;
  ctx.beginPath();
  ctx.moveTo(cx, cy - r);
  ctx.lineTo(cx - r * 0.4, cy - r * 0.4);
  ctx.lineTo(cx + r * 0.4, cy - r * 0.4);
  ctx.closePath();
  ctx.fill();

  // "N" 标注。
  ctx.fillStyle = palette.north;
  ctx.font = `bold ${Math.round(cellSize * 0.55)}px sans-serif`;
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText("N", cx, cy - r - cellSize * 0.32);
}

// 紫色航线：已走轨迹的「彗尾 + 粒子」效果（签名动效）。
// 只保留最近 MAX_TRAIL 格，旧轨迹随机器人移动自然淡出消失；粒子带呼吸脉冲（动态）。
const MAX_TRAIL = 8;

function drawCourse(
  ctx: CanvasRenderingContext2D,
  visited: Position[],
  cellSize: number,
  now = 0,
): void {
  if (visited.length < 2) {
    return;
  }
  // 尾巴 = 最近 MAX_TRAIL 格；head = 最新位置（数组尾），tail = 最旧位置（数组头）。
  const trail =
    visited.length <= MAX_TRAIL
      ? visited
      : visited.slice(visited.length - MAX_TRAIL);
  const m = trail.length;
  const pts = trail.map((p) => cellCenter(p, cellSize));

  // 1) 细而淡的连线（彗尾脊柱）：圆角 + 低透明度，弱化折线的硬直角。
  ctx.strokeStyle = palette.course;
  ctx.lineWidth = Math.max(2, cellSize * 0.06);
  ctx.lineJoin = "round";
  ctx.lineCap = "round";
  ctx.globalAlpha = 0.3;
  ctx.beginPath();
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 1; i < m; i++) {
    ctx.lineTo(pts[i].x, pts[i].y);
  }
  ctx.stroke();

  // 2) 粒子：每个轨迹格一个发光圆点（外光晕 + 亮核），头亮尾淡、头大尾小，呼吸脉冲。
  for (let i = 0; i < m; i++) {
    const a = (m - 1 - i) / (m - 1); // 1=最新(头) → 0=最旧(尾)
    const pulse = 0.7 + 0.3 * Math.sin(now * 0.005 - i * 0.9);
    const alpha = Math.max(0.05, (0.08 + 0.92 * a) * pulse);
    const r = cellSize * (0.045 + 0.105 * a);

    // 外光晕：大而淡，营造柔光。
    ctx.fillStyle = palette.course;
    ctx.globalAlpha = alpha * 0.35;
    ctx.beginPath();
    ctx.arc(pts[i].x, pts[i].y, r * 2.2, 0, Math.PI * 2);
    ctx.fill();

    // 亮核：小而实。
    ctx.globalAlpha = Math.min(1, alpha);
    ctx.beginPath();
    ctx.arc(pts[i].x, pts[i].y, r, 0, Math.PI * 2);
    ctx.fill();
  }

  // 复位透明度，避免影响后续绘制（机器人/感知层）。
  ctx.globalAlpha = 1;
}

// 地形质感：确定性 per-cell 染色（约 12% 格略绿/略暖），
// 让等高线网格读作"地形"而非"加粗方格"。哈希稳定，不随重绘闪烁。
function drawTerrainTexture(
  ctx: CanvasRenderingContext2D,
  map: MapInfo,
  cellSize: number,
): void {
  for (let y = 0; y < map.height; y++) {
    for (let x = 0; x < map.width; x++) {
      const v = (x * 73856093 + y * 19349663) >>> 0;
      if (v % 17 < 2) {
        ctx.fillStyle = palette.terrainA;
        ctx.fillRect(x * cellSize, y * cellSize, cellSize, cellSize);
      } else if (v % 23 < 1) {
        ctx.fillStyle = palette.terrainB;
        ctx.fillRect(x * cellSize, y * cellSize, cellSize, cellSize);
      }
    }
  }
}

export function draw(
  ctx: CanvasRenderingContext2D,
  map: MapInfo,
  robot: RobotState | null,
  cellSize: number,
  visited?: Position[],
  now = 0,
): void {
  const width = map.width * cellSize;
  const height = map.height * cellSize;

  ctx.clearRect(0, 0, width, height);

  // 背景：可通行地形。
  ctx.fillStyle = palette.field;
  ctx.fillRect(0, 0, width, height);

  // 地形质感（网格之下）。
  drawTerrainTexture(ctx, map, cellSize);

  // 等高线网格：细棕线；每 5 格一条计曲线（略深）。
  for (let x = 1; x < map.width; x++) {
    ctx.strokeStyle = x % 5 === 0 ? palette.gridIndex : palette.grid;
    ctx.lineWidth = x % 5 === 0 ? 1.5 : 1;
    ctx.beginPath();
    ctx.moveTo(x * cellSize, 0);
    ctx.lineTo(x * cellSize, height);
    ctx.stroke();
  }
  for (let y = 1; y < map.height; y++) {
    ctx.strokeStyle = y % 5 === 0 ? palette.gridIndex : palette.grid;
    ctx.lineWidth = y % 5 === 0 ? 1.5 : 1;
    ctx.beginPath();
    ctx.moveTo(0, y * cellSize);
    ctx.lineTo(width, y * cellSize);
    ctx.stroke();
  }

  // 障碍物：密林绿 + 纹理斜线，机器人无法进入。
  for (const o of map.obstacles) {
    drawObstacle(ctx, o.x * cellSize, o.y * cellSize, cellSize);
  }

  // 外边界：深土色地图边框。
  ctx.strokeStyle = palette.boundary;
  ctx.lineWidth = 3;
  ctx.strokeRect(1.5, 1.5, width - 3, height - 3);

  // 紫色航线（已走轨迹，在机器人之下）。
  if (visited) {
    drawCourse(ctx, visited, cellSize, now);
  }

  // 指北针。
  drawNorthArrow(ctx, width, cellSize);

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
    // 白描边：保证在密林绿底上也清晰。
    ctx.strokeStyle = "#ffffff";
    ctx.lineWidth = 1.5;
    ctx.stroke();
    ctx.fillStyle = "#ffffff";
    ctx.font = `bold ${Math.round(cellSize * 0.5)}px sans-serif`;
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("!", center.x, center.y + r * 0.12);
  }
}
