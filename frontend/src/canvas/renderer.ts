// Canvas 2D 渲染器：定向越野地图——可通行地形、等高线网格、密林障碍、
// 深土边界、紫色航线（已走轨迹）、紫色控制点机器人、指北针；感知异常图层。
// 不依赖 DOM 格子；坐标换算 pixelX = (x + 0.5) * cellSize。
import { severityColor } from "../format";
import { palette } from "../theme";
import type {
  Detection,
  Heading,
  MapInfo,
  Position,
  RobotState,
  TrailPoint,
} from "../types";

export const DEFAULT_CELL_SIZE = 32;

// 碰撞反馈：受阻格（越界时钳制到地图边缘格）+ 阻塞发生时刻（performance.now），
// 驱动红环扩散动画（见 drawBlockedFeedback）。
export interface BlockedFeedback {
  cell: Position;
  at: number;
}

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

// 受阻格：机器人朝向的下一格（碰撞点）。越界（BLOCKED_BOUNDARY）时钳制到地图
// 边缘格，让红环落在机器人贴边的位置，真实反映碰撞发生在哪一侧。
export function blockedCell(robot: RobotState, map: MapInfo): Position {
  const dir = headingVector(robot.heading);
  return {
    x: Math.max(0, Math.min(map.width - 1, robot.position.x + dir.x)),
    y: Math.max(0, Math.min(map.height - 1, robot.position.y + dir.y)),
  };
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
// 彗尾轨迹：按时间存活——每个轨迹点带到达时刻 at，超过 TRAIL_TTL 自动淡出，
// 因此机器人停下后轨迹会在 ~1.2s 内消失；头部粗而亮、尾部细而淡（由粗到细的渐变光迹）。
const TRAIL_TTL = 1200;

function drawCourse(
  ctx: CanvasRenderingContext2D,
  trail: TrailPoint[],
  cellSize: number,
  now = 0,
  reduceMotion = false,
): void {
  // 只保留仍在寿命内的点（now 来自 requestAnimationFrame，与 at 同钟）。
  const alive = trail.filter((p) => now - p.at <= TRAIL_TTL);
  if (alive.length < 2) {
    return;
  }
  const m = alive.length;
  const pts = alive.map((p) => cellCenter(p, cellSize));

  ctx.lineCap = "round";
  ctx.lineJoin = "round";
  ctx.strokeStyle = palette.course;
  ctx.shadowColor = palette.course;

  // 1) 逐段连线：越靠近机器人越粗越亮，越往尾越细越淡（由粗到细、由亮到淡的平滑渐变）。
  for (let i = 0; i < m - 1; i++) {
    const age = Math.max(0, now - alive[i + 1].at) / TRAIL_TTL; // 段头（较新端点）年龄 0→1
    const a = 1 - age;
    const alpha = Math.pow(a, 1.4) * 0.9;
    if (alpha < 0.02) {
      continue;
    }
    ctx.lineWidth = Math.max(0.8, cellSize * (0.02 + 0.22 * a));
    ctx.globalAlpha = alpha;
    // 只在靠近头部的段加柔光，尾部不加，避免整条糊成一片。
    ctx.shadowBlur = a > 0.5 ? cellSize * 0.32 * a : 0;
    ctx.beginPath();
    ctx.moveTo(pts[i].x, pts[i].y);
    ctx.lineTo(pts[i + 1].x, pts[i + 1].y);
    ctx.stroke();
  }

  // 2) 头部亮核：机器人最新位置上一颗小而实的发光点。
  const head = alive[m - 1];
  const headAge = Math.max(0, now - head.at) / TRAIL_TTL;
  const headA = 1 - headAge;
  ctx.globalAlpha = 0.92 * Math.pow(headA, 1.4);
  ctx.shadowBlur = cellSize * 0.55 * headA;
  ctx.fillStyle = palette.course;
  ctx.beginPath();
  ctx.arc(pts[m - 1].x, pts[m - 1].y, cellSize * 0.12, 0, Math.PI * 2);
  ctx.fill();

  // 3) 头部火花：几颗沿固定方向向外漂散的小粒子（leibox 粒子灵感，克制克量）。
  // 纯过程化、无状态：随 now 循环向外飞溅并淡出，机器人停下后随头部一起消散。
  // prefers-reduced-motion 下跳过（装饰性粒子，非必要动效）。
  if (!reduceMotion) {
    ctx.shadowBlur = 0;
    ctx.fillStyle = palette.course;
    const emberCount = 5;
    for (let i = 0; i < emberCount; i++) {
      const cycle = (now * 0.0016 + i * 0.23) % 1; // 每颗错开相位，0→1 循环
      const angle = (i / emberCount) * Math.PI * 2 + 0.7;
      const dist = cellSize * (0.18 + 0.5 * easeOutCubic(cycle));
      const ex = pts[m - 1].x + Math.cos(angle) * dist;
      const ey = pts[m - 1].y + Math.sin(angle) * dist;
      const ea = headA * 0.5 * (1 - cycle);
      if (ea < 0.02) {
        continue;
      }
      ctx.globalAlpha = ea;
      ctx.beginPath();
      ctx.arc(ex, ey, Math.max(0.5, cellSize * 0.055), 0, Math.PI * 2);
      ctx.fill();
    }
  }

  // 复位透明度与阴影，避免影响后续绘制（机器人/感知层）。
  ctx.globalAlpha = 1;
  ctx.shadowBlur = 0;
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

// 缓动：指数式 ease-out（craft-floor 的动效基调），红环/火花共用。
function easeOutCubic(t: number): number {
  return 1 - Math.pow(1 - t, 3);
}

// 碰撞反馈：受阻格上的红环扩散 + 格内红晕 + 向外飞溅的火花。
// 与 useRobotController 的 BOUNDARY_FEEDBACK_MS(1500) 对齐，一次性扩散淡出。
const BLOCKED_TTL = 1500;

function drawBlockedFeedback(
  ctx: CanvasRenderingContext2D,
  blocked: BlockedFeedback,
  cellSize: number,
  now: number,
  reduceMotion = false,
): void {
  const age = now - blocked.at;
  if (age < 0 || age > BLOCKED_TTL) {
    return;
  }
  const t = age / BLOCKED_TTL; // 0→1
  const center = cellCenter(blocked.cell, cellSize);

  // prefers-reduced-motion：静态化——固定红晕 + 一圈固定红环，不做扩散/火花。
  // 仍保留「在受阻格标出碰撞位置」这一真实反馈，只是去掉运动。
  if (reduceMotion) {
    ctx.fillStyle = palette.danger;
    ctx.globalAlpha = 0.2;
    ctx.fillRect(
      blocked.cell.x * cellSize,
      blocked.cell.y * cellSize,
      cellSize,
      cellSize,
    );
    ctx.strokeStyle = palette.danger;
    ctx.lineWidth = 2;
    ctx.globalAlpha = 0.85;
    ctx.beginPath();
    ctx.arc(center.x, center.y, cellSize * 0.72, 0, Math.PI * 2);
    ctx.stroke();
    ctx.globalAlpha = 1;
    return;
  }

  // 1) 受阻格红晕：快速闪现后淡出（真实标出是哪一格被挡住）。
  const flash = Math.max(0, 1 - t * 2.2);
  if (flash > 0) {
    ctx.fillStyle = palette.danger;
    ctx.globalAlpha = flash * 0.2;
    ctx.fillRect(
      blocked.cell.x * cellSize,
      blocked.cell.y * cellSize,
      cellSize,
      cellSize,
    );
  }

  // 2) 红环扩散：两圈错开的环从碰撞点向外扩散并淡出（红环扩散动画）。
  for (let ring = 0; ring < 2; ring++) {
    const start = ring * 0.12;
    const rt = (t - start) / (1 - start);
    if (rt < 0 || rt >= 1) {
      continue;
    }
    const radius = cellSize * (0.3 + 1.5 * easeOutCubic(rt));
    ctx.strokeStyle = palette.danger;
    ctx.lineWidth = 2.5 * (1 - rt) + 0.5;
    ctx.globalAlpha = 0.85 * (1 - rt);
    ctx.beginPath();
    ctx.arc(center.x, center.y, radius, 0, Math.PI * 2);
    ctx.stroke();
  }

  // 3) 火花：沿固定方向向外飞溅的小红点，随扩散淡出。
  const sparkCount = 8;
  for (let i = 0; i < sparkCount; i++) {
    const angle = (i / sparkCount) * Math.PI * 2 + 0.4;
    const dist = cellSize * (0.5 + 1.7 * easeOutCubic(t));
    const sx = center.x + Math.cos(angle) * dist;
    const sy = center.y + Math.sin(angle) * dist;
    const sa = 0.7 * (1 - t);
    ctx.fillStyle = palette.danger;
    ctx.globalAlpha = sa;
    ctx.beginPath();
    ctx.arc(sx, sy, Math.max(0.5, cellSize * 0.09 * (1 - t)), 0, Math.PI * 2);
    ctx.fill();
  }

  // 复位透明度，避免影响后续绘制。
  ctx.globalAlpha = 1;
}

export function draw(
  ctx: CanvasRenderingContext2D,
  map: MapInfo,
  robot: RobotState | null,
  cellSize: number,
  visited?: TrailPoint[],
  now = 0,
  blocked?: BlockedFeedback | null,
  reduceMotion = false,
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
    drawCourse(ctx, visited, cellSize, now, reduceMotion);
  }

  // 指北针。
  drawNorthArrow(ctx, width, cellSize);

  if (robot) {
    drawRobot(ctx, robot, cellSize);
  }

  // 碰撞反馈：压在最上层，红环扩散只在受阻反馈窗口内绘制。
  if (blocked) {
    drawBlockedFeedback(ctx, blocked, cellSize, now, reduceMotion);
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
