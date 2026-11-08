// UI 状态机：LOADING/READY/EXECUTING/BOUNDARY_FEEDBACK/DEGRADED/DISCONNECTED。
// 纯 reducer，便于单元测试；副作用（fetch、定时器）由 main.ts 编排。
import type { CommandResult, MapInfo, RobotState } from "./types";

export type UiState =
  | "LOADING"
  | "READY"
  | "EXECUTING"
  | "BOUNDARY_FEEDBACK"
  | "DEGRADED"
  | "DISCONNECTED";

export interface AppState {
  ui: UiState;
  map: MapInfo | null;
  robot: RobotState | null;
  pollFailures: number;
  // 标记检测到 instanceId 变化，调用方需重新拉取地图与状态。
  instanceChanged: boolean;
  // 巡检期间最近一次受阻的报警锁存（非 null 表示正在报警）。由
  // main.ts 定时器在 PATROL_ALERT_MS 后清除，避免 250ms 巡检间隔下
  // 报警态（BLOCKED_OBSTACLE/BLOCKED_BOUNDARY）被下一步 MOVED 秒覆盖。
  patrolAlert: "BLOCKED_OBSTACLE" | "BLOCKED_BOUNDARY" | null;
}

export type AppEvent =
  | { type: "loading" }
  | { type: "loaded"; map: MapInfo; robot: RobotState }
  | { type: "pollOk"; robot: RobotState }
  | { type: "pollFail" }
  | { type: "disconnect" }
  | { type: "commandStart" }
  | { type: "commandDone"; robot: RobotState; result: CommandResult }
  | { type: "feedbackTimeout" }
  | { type: "patrolAlertTimeout" };

export const MAX_POLL_FAILURES = 3;

// 巡检期间受阻（障碍/边界）的报警锁存：本次轮询若仍受阻则记录类型；
// 巡检结束（patrolActive 变 false）则清除；否则保持旧值（锁存报警，
// 避免被下一步 MOVED 立即覆盖）。
function patrolAlertFor(
  robot: RobotState,
  prev: "BLOCKED_OBSTACLE" | "BLOCKED_BOUNDARY" | null,
): "BLOCKED_OBSTACLE" | "BLOCKED_BOUNDARY" | null {
  if (!robot.patrolActive) {
    return null;
  }
  if (
    robot.lastResult === "BLOCKED_OBSTACLE" ||
    robot.lastResult === "BLOCKED_BOUNDARY"
  ) {
    return robot.lastResult;
  }
  return prev;
}

export function createInitialState(): AppState {
  return {
    ui: "LOADING",
    map: null,
    robot: null,
    pollFailures: 0,
    instanceChanged: false,
    patrolAlert: null,
  };
}

export function reduce(state: AppState, event: AppEvent): AppState {
  switch (event.type) {
    case "loading":
      return { ...state, ui: "LOADING", instanceChanged: false, patrolAlert: null };

    case "loaded":
      return {
        ...state,
        ui: "READY",
        map: event.map,
        robot: event.robot,
        pollFailures: 0,
        instanceChanged: false,
        patrolAlert: null,
      };

    case "pollOk": {
      const prev = state.robot;
      // 服务重启：instanceId 变化，进入 LOADING 并触发重载。
      if (prev && event.robot.instanceId !== prev.instanceId) {
        return {
          ...state,
          ui: "LOADING",
          robot: null,
          instanceChanged: true,
          patrolAlert: null,
        };
      }
      // 同实例但序列号更小：忽略过期状态。
      if (prev && event.robot.sequence < prev.sequence) {
        return state;
      }
      // 命令执行中收到轮询：只更新数据，保持 EXECUTING 以免按钮提前放开。
      const ui: UiState = state.ui === "EXECUTING" ? "EXECUTING" : "READY";
      return {
        ...state,
        ui,
        robot: event.robot,
        pollFailures: 0,
        patrolAlert: patrolAlertFor(event.robot, state.patrolAlert),
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
      return {
        ...state,
        ui: "DISCONNECTED",
        pollFailures: MAX_POLL_FAILURES,
        patrolAlert: null,
      };

    case "commandStart":
      return { ...state, ui: "EXECUTING" };

    case "commandDone": {
      const prev = state.robot;
      // 防御性：若响应比当前快照旧则丢弃，仅回到 READY。
      if (prev && event.robot.sequence < prev.sequence) {
        return { ...state, ui: "READY" };
      }
      const ui: UiState =
        event.result === "BLOCKED_BOUNDARY" || event.result === "BLOCKED_OBSTACLE"
          ? "BOUNDARY_FEEDBACK"
          : "READY";
      return {
        ...state,
        ui,
        robot: event.robot,
        pollFailures: 0,
        patrolAlert: patrolAlertFor(event.robot, state.patrolAlert),
      };
    }

    case "feedbackTimeout":
      return { ...state, ui: "READY" };

    case "patrolAlertTimeout":
      return { ...state, patrolAlert: null };
  }
}
