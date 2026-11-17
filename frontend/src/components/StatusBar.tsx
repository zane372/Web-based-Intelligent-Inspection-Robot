import { lampClass, robotLampTone, statusText } from "../format";
import type { AppState } from "../state";

export function StatusBar({ state }: { state: AppState }) {
  const robot = state.robot;
  const pulsing =
    state.robot?.patrolActive === true && state.patrolAlert !== null;

  return (
    <header className="flex flex-wrap items-end justify-between gap-5">
      <div className="flex flex-col gap-2">
        <h1 className="m-0 flex items-center gap-3 text-[30px] leading-tight font-bold tracking-tight text-ink">
          <span className="course-mark" aria-hidden="true" />
          智能巡检机器人
        </h1>
        <p className="m-0 text-sm text-ink-muted">
          二维网格地图仿真 · 手动控制与自动巡检
        </p>
      </div>

      <div className="flex items-center gap-3">
        {robot && (
          <span className="hidden items-center gap-2 rounded-full border border-line-strong bg-surface px-3 py-1.5 text-xs font-semibold text-ink-soft min-[900px]:inline-flex">
            机器人
            <span className="readout-value">{robot.robotId}</span>
          </span>
        )}
        <div
          className="status-pill"
          role="status"
          aria-live="polite"
          data-state={state.ui}
        >
          <span className={lampClass(robotLampTone(state), pulsing)} />
          {statusText(state)}
        </div>
      </div>
    </header>
  );
}
