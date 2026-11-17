import type { ReactNode } from "react";
import {
  headingLabel,
  lastResultLabel,
  motionStatusLabel,
} from "../format";
import type { AppState } from "../state";
import type { Heading } from "../types";
import {
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  PlayIcon,
  ReconnectIcon,
  ResetIcon,
  StopIcon,
} from "./icons";

// heading → 中央罗盘指针角度（北=上=0°，顺时针）。
function headingAngle(h: Heading): number {
  switch (h) {
    case "UP":
      return 0;
    case "RIGHT":
      return 90;
    case "DOWN":
      return 180;
    case "LEFT":
      return 270;
  }
}

interface Props {
  state: AppState;
  onMove: (direction: Heading) => void;
  onReset: () => void;
  onTogglePatrol: () => void;
  onReconnect: () => void;
}

function DirButton({
  label,
  disabled,
  onClick,
  children,
}: {
  label: string;
  disabled: boolean;
  onClick: () => void;
  children: ReactNode;
}) {
  return (
    <button
      type="button"
      className="dir-btn"
      aria-label={label}
      disabled={disabled}
      onClick={onClick}
    >
      {children}
    </button>
  );
}

export function ControlPanel({
  state,
  onMove,
  onReset,
  onTogglePatrol,
  onReconnect,
}: Props) {
  const robot = state.robot;
  const canCommand = state.ui === "READY" || state.ui === "BOUNDARY_FEEDBACK";
  const patrolling = robot?.patrolActive ?? false;

  const readout: { label: string; value: string }[] = [
    { label: "位置", value: robot ? `(${robot.position.x}, ${robot.position.y})` : "—" },
    { label: "朝向", value: robot ? headingLabel(robot.heading) : "—" },
    { label: "运行状态", value: robot ? motionStatusLabel(robot.motionStatus) : "—" },
    { label: "最近结果", value: robot ? lastResultLabel(robot.lastResult) : "—" },
    { label: "序列号", value: robot ? String(robot.sequence) : "—" },
  ];

  return (
    <aside aria-label="控制区" className="flex flex-col gap-5">
      <div className="panel flex flex-col gap-3 p-5">
        <h2 className="m-0 text-base font-bold text-ink">运行读数</h2>
        <dl className="m-0 flex flex-col gap-2.5 text-sm">
          {readout.map((r) => (
            <div key={r.label} className="readout-row">
              <dt className="text-ink-muted">{r.label}</dt>
              <dd className="readout-value m-0">{r.value}</dd>
            </div>
          ))}
        </dl>
      </div>

      <div className="panel flex flex-col items-center gap-4 p-5">
        <h2 className="control-label m-0 self-start">方向控制</h2>
        <div className="flex flex-col items-center gap-3">
          <DirButton
            label="向上移动"
            disabled={!canCommand}
            onClick={() => onMove("UP")}
          >
            <ArrowUp />
          </DirButton>
          <div className="flex items-center gap-3">
            <DirButton
              label="向左移动"
              disabled={!canCommand}
              onClick={() => onMove("LEFT")}
            >
              <ArrowLeft />
            </DirButton>
            {/* 中央罗盘：显示当前朝向（指针随 heading 旋转）。 */}
            <span
              className="inline-flex h-[52px] w-[52px] items-center justify-center rounded-full border border-line bg-surface-soft"
              role="img"
              aria-label={`当前朝向：${robot ? headingLabel(robot.heading) : "未知"}`}
            >
              <svg
                width="18"
                height="18"
                viewBox="0 0 18 18"
                className="text-course"
                style={{
                  transform: `rotate(${robot ? headingAngle(robot.heading) : 0}deg)`,
                  transition: "transform 0.2s ease",
                }}
                aria-hidden="true"
              >
                <path d="M9 2 5 16h8L9 2z" fill="currentColor" />
              </svg>
            </span>
            <DirButton
              label="向右移动"
              disabled={!canCommand}
              onClick={() => onMove("RIGHT")}
            >
              <ArrowRight />
            </DirButton>
          </div>
          <DirButton
            label="向下移动"
            disabled={!canCommand}
            onClick={() => onMove("DOWN")}
          >
            <ArrowDown />
          </DirButton>
        </div>
        <button
          type="button"
          className="action-btn w-full"
          aria-label="重置位置"
          disabled={!canCommand}
          onClick={onReset}
        >
          <ResetIcon className="h-4 w-4" />
          重置位置
        </button>
      </div>

      <button
        type="button"
        className={`patrol-btn ${patrolling ? "is-active" : ""}`}
        aria-label={patrolling ? "停止巡检" : "自动巡检"}
        disabled={!canCommand}
        onClick={onTogglePatrol}
      >
        {patrolling ? (
          <StopIcon className="h-5 w-5" />
        ) : (
          <PlayIcon className="h-5 w-5" />
        )}
        {patrolling ? "停止巡检" : "自动巡检"}
      </button>

      {state.ui === "DISCONNECTED" && (
        <button
          type="button"
          className="reconnect-btn"
          aria-label="重新连接"
          onClick={onReconnect}
        >
          <ReconnectIcon className="h-4 w-4" />
          重新连接
        </button>
      )}

      <section className="panel p-5 text-sm" aria-label="使用帮助">
        <h2 className="control-label mb-2.5">使用帮助</h2>
        <ul className="m-0 list-disc space-y-1.5 pl-5 leading-relaxed text-ink-soft">
          <li>每次移动一格。</li>
          <li>越界不会离开地图，机器人停在原地。</li>
          <li>绿色格子为密林障碍，机器人无法进入。</li>
          <li>「自动巡检」可让机器人自动遍历地图。</li>
          <li>方向键 / WASD 移动，空格切换巡检。</li>
          <li>坐标原点 (0,0) 位于左上角。</li>
        </ul>
      </section>
    </aside>
  );
}
