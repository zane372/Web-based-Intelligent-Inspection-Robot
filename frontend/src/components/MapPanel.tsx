import { useEffect, useRef, useState } from "react";
import type { ReactNode } from "react";
import {
  DEFAULT_CELL_SIZE,
  draw,
  drawPerceptionLayer,
  setupCanvas,
} from "../canvas/renderer";
import { headingLabel } from "../format";
import type { UiState } from "../state";
import { palette } from "../theme";
import type { Detection, MapInfo, Position, RobotState } from "../types";

interface Props {
  className?: string;
  map: MapInfo | null;
  robot: RobotState | null;
  ui: UiState;
  discovered: Detection[];
  visited: Position[];
}

function LegendItem({ swatch, label }: { swatch: ReactNode; label: string }) {
  return (
    <li className="inline-flex items-center gap-2 text-xs text-ink-soft">
      <span
        className="inline-flex h-4 w-4 items-center justify-center"
        aria-hidden="true"
      >
        {swatch}
      </span>
      {label}
    </li>
  );
}

export function MapPanel({
  className,
  map,
  robot,
  ui,
  discovered,
  visited,
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const ctxRef = useRef<CanvasRenderingContext2D | null>(null);
  const [dprKey, setDprKey] = useState(0);

  // 跨 DPI 显示器拖动时 devicePixelRatio 会变化；监听分辨率变化触发重采样。
  useEffect(() => {
    let mql = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
    const onChange = (): void => {
      mql.removeEventListener("change", onChange);
      mql = window.matchMedia(`(resolution: ${window.devicePixelRatio}dppx)`);
      mql.addEventListener("change", onChange);
      setDprKey((k) => k + 1);
    };
    mql.addEventListener("change", onChange);
    return () => mql.removeEventListener("change", onChange);
  }, []);

  // 地图变化或 DPR 变化时（重新）初始化画布并绘制一次。
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !map) {
      ctxRef.current = null;
      return;
    }
    ctxRef.current = setupCanvas(canvas, map, DEFAULT_CELL_SIZE);
    const ctx = ctxRef.current;
    if (ctx) {
      draw(ctx, map, robot, DEFAULT_CELL_SIZE, visited);
      drawPerceptionLayer(ctx, discovered, DEFAULT_CELL_SIZE);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [map, dprKey]);

  // 机器人/已发现异常/已走轨迹变化时重绘。
  useEffect(() => {
    const ctx = ctxRef.current;
    if (!ctx || !map) {
      return;
    }
    draw(ctx, map, robot, DEFAULT_CELL_SIZE, visited);
    drawPerceptionLayer(ctx, discovered, DEFAULT_CELL_SIZE);
  }, [robot, discovered, map, visited]);

  const blocked = ui === "BOUNDARY_FEEDBACK";
  const mapSummary = map
    ? `${map.width}×${map.height} 网格地图${
        robot
          ? `，机器人在 (${robot.position.x},${robot.position.y})，朝向${headingLabel(
              robot.heading,
            )}`
          : ""
      }${discovered.length > 0 ? `，已发现 ${discovered.length} 处异常` : ""}`
    : "机器人巡检地图";

  return (
    <section
      aria-label="地图区"
      className={`flex h-full min-h-0 min-w-0 flex-col gap-5 ${className ?? ""}`}
    >
      <h2 className="sr-only">地图</h2>
      <div className="flex min-h-0 flex-1 items-center justify-center">
        <div className={`map-frame ${blocked ? "is-blocked" : ""}`}>
          <canvas
            ref={canvasRef}
            role="img"
            aria-label={mapSummary}
            className="block h-full w-full"
          />
        </div>
      </div>

      {/* 符号图例 + 比例尺 + 地图信息，与地图同宽居中（不铺满列宽，避免与地图错位）。 */}
      <div className="map-legend mx-auto flex w-full max-w-[calc(100vh-200px)] shrink-0 flex-col gap-2.5">
        <h3 className="control-label m-0">符号图例</h3>
        <ul
          className="m-0 flex list-none flex-wrap items-center gap-x-6 gap-y-2 pl-0"
          aria-label="图例"
        >
          <LegendItem
            label="障碍（密林）"
            swatch={
              <svg width="16" height="16" viewBox="0 0 16 16">
                <rect x="1" y="1" width="14" height="14" fill={palette.obstacle} />
                <path d="M3 13 13 3" stroke={palette.obstacleHatch} strokeWidth="2" />
                <path d="M8 14 14 8" stroke={palette.obstacleHatch} strokeWidth="2" />
              </svg>
            }
          />
          <LegendItem
            label="机器人（控制点）"
            swatch={
              <svg width="16" height="16" viewBox="0 0 16 16">
                <circle
                  cx="8"
                  cy="8"
                  r="6"
                  fill="none"
                  stroke={palette.course}
                  strokeWidth="2"
                />
                <circle cx="8" cy="8" r="3" fill={palette.robotBody} />
              </svg>
            }
          />
          <LegendItem
            label="已发现异常"
            swatch={
              <svg width="16" height="16" viewBox="0 0 16 16">
                <path d="M8 2 1.5 14h13L8 2z" fill={palette.danger} />
              </svg>
            }
          />
          <LegendItem
            label="地图边界"
            swatch={
              <svg width="16" height="16" viewBox="0 0 16 16">
                <rect
                  x="1.5"
                  y="1.5"
                  width="13"
                  height="13"
                  fill="none"
                  stroke={palette.boundary}
                  strokeWidth="2.5"
                />
              </svg>
            }
          />
        </ul>

        <div className="flex flex-wrap items-center gap-x-6 gap-y-2 text-xs text-ink-soft">
          <span className="inline-flex items-center gap-2">
            比例尺
            <span className="inline-flex items-center gap-1.5" aria-label="10 格比例尺">
              <span className="h-2 w-10 rounded-sm bg-ink" />
              <span className="readout-value">10 格</span>
            </span>
          </span>
          <span className="inline-flex items-center gap-2">
            指北
            <svg width="12" height="16" viewBox="0 0 12 16" aria-hidden="true">
              <path d="M6 15 6 2" stroke="#4a5644" strokeWidth="2" />
              <path d="M6 2 2.8 6h6.4L6 2z" fill="#4a5644" />
            </svg>
            上
          </span>
          <span>
            地图 {map ? `${map.width}×${map.height}` : "—"} · 障碍{" "}
            {map ? map.obstacles.length : "—"} · 原点 (0,0) 左上
          </span>
        </div>
      </div>
    </section>
  );
}
