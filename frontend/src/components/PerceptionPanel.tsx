import {
  eventText,
  hazardTypeText,
  lampClass,
  perceptionLampTone,
  perceptionStatusText,
  rangeText,
  severityColor,
  severityText,
} from "../format";
import type { PerceptionState } from "../perception_state";
import { WarningIcon } from "./icons";

export function PerceptionPanel({ state }: { state: PerceptionState }) {
  const obs = state.observation;
  const byRel = obs
    ? new Map(obs.ranges.map((r) => [r.relativeDirection, r]))
    : new Map();

  const ranges = [
    { label: "前", value: rangeText(byRel.get("FRONT")) },
    { label: "左", value: rangeText(byRel.get("LEFT")) },
    { label: "右", value: rangeText(byRel.get("RIGHT")) },
  ];

  return (
    <section
      aria-label="虚拟感知"
      className="panel flex flex-col gap-5 p-6 text-sm"
    >
      <div className="flex items-center justify-between gap-4">
        <h2 className="m-0 text-base font-bold text-ink">虚拟感知</h2>
        <p
          className="m-0 inline-flex items-center gap-2 font-semibold text-ink-soft"
          role="status"
          aria-live="polite"
          data-state={state.ui}
        >
          <span className={lampClass(perceptionLampTone(state))} />
          {perceptionStatusText(state)}
        </p>
      </div>

      <div className="flex flex-col gap-2.5">
        <h3 className="control-label m-0">三向测距</h3>
        {ranges.map((r) => (
          <div key={r.label} className="readout-row">
            <span className="text-ink-muted">{r.label}</span>
            <span className="readout-value">{r.value}</span>
          </div>
        ))}
        <div className="readout-row">
          <span className="text-ink-muted">观察序号</span>
          <span className="readout-value">
            {obs ? String(obs.sequence) : "—"}
          </span>
        </div>
      </div>

      <div className="flex flex-col gap-2">
        <h3 className="control-label m-0">活动异常</h3>
        <ul
          className="m-0 flex max-h-60 list-none flex-col gap-2 overflow-y-auto pl-0"
          tabIndex={0}
        >
          {state.activeDetections.length === 0 ? (
            <li className="text-ink-muted">无</li>
          ) : (
            state.activeDetections.map((d) => (
              <li key={d.hazardId} className="flex items-center gap-2 text-ink-soft">
                <WarningIcon
                  className="h-4 w-4 shrink-0"
                  style={{ color: severityColor(d.severity) }}
                />
                <span>
                  {hazardTypeText(d.hazardType)}（{severityText(d.severity)}）@
                  ({d.position.x},{d.position.y})，距离 {d.distanceCells}
                </span>
              </li>
            ))
          )}
        </ul>
      </div>

      <div className="flex flex-col gap-2">
        <h3 className="control-label m-0">感知事件</h3>
        <ul
          className="m-0 flex max-h-60 list-none flex-col gap-2 overflow-y-auto pl-0"
          aria-live="polite"
          tabIndex={0}
        >
          {state.truncated && (
            <li className="text-ink-muted">事件已截断，正在重建…</li>
          )}
          {state.events.map((e) => (
            <li key={e.eventId} className="text-ink-soft">
              <span className="readout-value mr-2">#{e.eventId}</span>
              {eventText(e)}
            </li>
          ))}
          {state.events.length === 0 && !state.truncated && (
            <li className="text-ink-muted">暂无事件</li>
          )}
        </ul>
      </div>
    </section>
  );
}
