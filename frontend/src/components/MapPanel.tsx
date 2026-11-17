import { useEffect, useRef, useState } from "react";
import type { CSSProperties, ReactNode } from "react";
import {
  DEFAULT_CELL_SIZE,
  draw,
  drawPerceptionLayer,
  setupCanvas,
} from "../canvas/renderer";
import { headingLabel } from "../format";
import type { UiState } from "../state";
import type { Detection, MapInfo, RobotState } from "../types";

interface Props {
  className?: string;
  map: MapInfo | null;
  robot: RobotState | null;
  ui: UiState;
  discovered: Detection[];
}

function LegendItem({ swatch, label }: { swatch: ReactNode; label: string }) {
  return (
    <span className="inline-flex items-center gap-2 text-xs text-ink-soft">
      <span className="inline-flex h-4 w-4 items-center justify-center">
        {swatch}
      </span>
      {label}
    </span>
  );
}

export function MapPanel({ className, map, robot, ui, discovered }: Props) {
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
      draw(ctx, map, robot, DEFAULT_CELL_SIZE);
      drawPerceptionLayer(ctx, discovered, DEFAULT_CELL_SIZE);
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [map, dprKey]);

  // 机器人/已发现异常变化时重绘。
  useEffect(() => {
    const ctx = ctxRef.current;
    if (!ctx || !map) {
      return;
    }
    draw(ctx, map, robot, DEFAULT_CELL_SIZE);
    drawPerceptionLayer(ctx, discovered, DEFAULT_CELL_SIZE);
  }, [robot, discovered, map]);

  const blocked = ui === "BOUNDARY_FEEDBACK";
  const canvasStyle: CSSProperties = {
    maxWidth: "min(100%, calc(100vh - 250px))",
  };
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
      className={`flex min-w-0 flex-col gap-3 ${className ?? ""}`}
    >
      <h2 className="sr-only">地图</h2>
      <div className={`map-frame ${blocked ? "is-blocked" : ""}`}>
        <canvas
          ref={canvasRef}
          role="img"
          aria-label={mapSummary}
          className="block h-auto w-full"
          style={canvasStyle}
        />
      </div>

      {/* 图例：图形 + 文字，非仅颜色。 */}
      <div
        className="flex flex-wrap items-center gap-x-6 gap-y-2"
        aria-label="图例"
      >
        <LegendItem
          label="障碍（禁止进入）"
          swatch={
            <svg width="16" height="16" viewBox="0 0 16 16">
              <rect x="1" y="1" width="14" height="14" fill="#26303c" />
              <path d="M2.5 13.5 13.5 2.5" stroke="#e2a50c" strokeWidth="2" />
            </svg>
          }
        />
        <LegendItem
          label="机器人"
          swatch={
            <svg width="16" height="16" viewBox="0 0 16 16">
              <circle cx="8" cy="8" r="5.5" fill="#1b2531" />
              <path d="M8 3.5 11 8 8 12.5 5 8z" fill="#e2a50c" />
            </svg>
          }
        />
        <LegendItem
          label="已发现异常"
          swatch={
            <svg width="16" height="16" viewBox="0 0 16 16">
              <path d="M8 2 1.5 14h13L8 2z" fill="#bd2a22" />
            </svg>
          }
        />
        <LegendItem
          label="地图边界"
          swatch={
            <svg width="16" height="16" viewBox="0 0 16 16">
              <rect x="1.5" y="1.5" width="13" height="13" fill="none" stroke="#1b2531" strokeWidth="2" />
              <rect x="4.5" y="4.5" width="7" height="7" fill="none" stroke="#d97706" strokeWidth="1.2" />
            </svg>
          }
        />
      </div>
    </section>
  );
}
