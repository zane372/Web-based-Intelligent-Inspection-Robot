import { lampClass, robotLampTone, statusText } from "../format";
import type { AppState } from "../state";

export function StatusBar({ state }: { state: AppState }) {
  const robot = state.robot;
  const map = state.map;
  const pulsing =
    state.robot?.patrolActive === true && state.patrolAlert !== null;

  return (
    <header className="flex flex-col gap-4">
      <div className="hazard-tape h-2.5 rounded-full" aria-hidden="true" />

      <div className="flex flex-wrap items-end justify-between gap-6">
        <div className="flex flex-col gap-1.5">
          <h1 className="m-0 text-[30px] leading-tight font-bold tracking-tight text-ink">
            智能巡检机器人
          </h1>
          <p className="m-0 text-sm text-ink-muted">
            二维网格地图仿真 · 手动控制与自动巡检
          </p>
        </div>

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

      {/* 现场信息牌 */}
      <dl className="m-0 flex flex-wrap gap-x-8 gap-y-2 text-sm text-ink-soft">
        <div className="flex items-center gap-2">
          <dt className="hazard-label">地图</dt>
          <dd className="readout-value m-0">
            {map ? `${map.width}×${map.height}` : "—"}
          </dd>
        </div>
        <div className="flex items-center gap-2">
          <dt className="hazard-label">障碍</dt>
          <dd className="readout-value m-0">
            {map ? map.obstacles.length : "—"}
          </dd>
        </div>
        <div className="flex items-center gap-2">
          <dt className="hazard-label">机器人</dt>
          <dd className="readout-value m-0">{robot?.robotId ?? "—"}</dd>
        </div>
        <div className="flex items-center gap-2">
          <dt className="hazard-label">坐标原点</dt>
          <dd className="readout-value m-0">左上角</dd>
        </div>
      </dl>
    </header>
  );
}
