// 感知面板独立状态机：DISABLED/LOADING/READY/DEGRADED/DISCONNECTED。
// 与机器人 UI 状态机解耦（SPEC 6.8）：感知故障不得断连手动/巡检。
import type {
  Detection,
  PerceptionEvent,
  PerceptionLatest,
  PerceptionObservation,
} from "./types";

export type PerceptionUiState =
  | "DISABLED"
  | "LOADING"
  | "READY"
  | "DEGRADED"
  | "DISCONNECTED";

export interface PerceptionState {
  ui: PerceptionUiState;
  instanceId: string | null;
  observation: PerceptionObservation | null;
  activeDetections: Detection[];
  // 已发现异常：跨观察累积（去重 hazardId）。感知图层只画这些，不得泄露
  // 未发现异常的位置（SPEC 6.8）。
  discovered: Detection[];
  events: PerceptionEvent[];
  after: number;
  latestEventId: number;
  truncated: boolean;
  pollFailures: number;
}

export type PerceptionAction =
  | { type: "loading" }
  | { type: "disabled" }
  | { type: "ready"; data: PerceptionLatest }
  | { type: "pollOk"; data: PerceptionLatest }
  | { type: "eventsLoaded"; data: { events: PerceptionEvent[]; latestEventId: number; truncated: boolean } }
  | { type: "pollFail" }
  | { type: "disconnect" };

export const MAX_POLL_FAILURES = 3;
export const MAX_EVENTS = 20;
export const EVENTS_LIMIT = 50;

function mergeDiscovered(
  existing: Detection[],
  incoming: Detection[],
): Detection[] {
  const map = new Map<string, Detection>();
  for (const d of existing) {
    map.set(d.hazardId, d);
  }
  for (const d of incoming) {
    map.set(d.hazardId, d);
  }
  return [...map.values()];
}

function mergeEvents(
  existing: PerceptionEvent[],
  incoming: PerceptionEvent[],
): PerceptionEvent[] {
  const seen = new Set<number>();
  for (const e of existing) {
    seen.add(e.eventId);
  }
  const merged = [...existing];
  for (const e of incoming) {
    if (!seen.has(e.eventId)) {
      seen.add(e.eventId);
      merged.push(e);
    }
  }
  // 按 eventId 升序，仅保留最近 MAX_EVENTS 条。
  merged.sort((a, b) => a.eventId - b.eventId);
  return merged.slice(-MAX_EVENTS);
}

function applyObservation(
  state: PerceptionState,
  data: PerceptionLatest,
  ui: PerceptionUiState,
): PerceptionState {
  const instanceChanged =
    state.instanceId !== null && state.instanceId !== data.instanceId;
  if (instanceChanged) {
    // 服务重启：清空事件游标与已发现异常，从 after=0 重建。
    return {
      ...state,
      ui,
      instanceId: data.instanceId,
      observation: data.observation,
      activeDetections: data.activeDetections,
      discovered: [...data.activeDetections],
      events: [],
      after: 0,
      latestEventId: data.latestEventId,
      truncated: false,
      pollFailures: 0,
    };
  }
  return {
    ...state,
    ui,
    instanceId: data.instanceId,
    observation: data.observation,
    activeDetections: data.activeDetections,
    discovered: mergeDiscovered(state.discovered, data.activeDetections),
    latestEventId: data.latestEventId,
    pollFailures: 0,
  };
}

export function createInitialPerceptionState(): PerceptionState {
  return {
    ui: "LOADING",
    instanceId: null,
    observation: null,
    activeDetections: [],
    discovered: [],
    events: [],
    after: 0,
    latestEventId: 0,
    truncated: false,
    pollFailures: 0,
  };
}

export function reducePerception(
  state: PerceptionState,
  action: PerceptionAction,
): PerceptionState {
  switch (action.type) {
    case "loading":
      return { ...createInitialPerceptionState(), ui: "LOADING" };

    case "disabled":
      return {
        ...createInitialPerceptionState(),
        ui: "DISABLED",
      };

    case "ready":
      return applyObservation(state, action.data, "READY");

    case "pollOk":
      return applyObservation(state, action.data, state.ui === "DISCONNECTED" ? "READY" : state.ui);

    case "eventsLoaded": {
      const truncated = action.data.truncated;
      if (truncated) {
        // 事件已被环形缓冲截断：清空事件列表，游标归零重建（并提示）。
        return {
          ...state,
          events: [],
          after: 0,
          latestEventId: action.data.latestEventId,
          truncated: true,
        };
      }
      return {
        ...state,
        events: mergeEvents(state.events, action.data.events),
        after: action.data.latestEventId,
        latestEventId: action.data.latestEventId,
        truncated: false,
      };
    }

    case "pollFail": {
      const failures = state.pollFailures + 1;
      if (failures >= MAX_POLL_FAILURES) {
        return { ...state, ui: "DISCONNECTED", pollFailures: failures };
      }
      return { ...state, ui: "DEGRADED", pollFailures: failures };
    }

    case "disconnect":
      return { ...state, ui: "DISCONNECTED", pollFailures: MAX_POLL_FAILURES };
  }
}
