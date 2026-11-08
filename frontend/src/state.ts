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
}

export type AppEvent =
  | { type: "loading" }
  | { type: "loaded"; map: MapInfo; robot: RobotState }
  | { type: "pollOk"; robot: RobotState }
  | { type: "pollFail" }
  | { type: "disconnect" }
  | { type: "commandStart" }
  | { type: "commandDone"; robot: RobotState; result: CommandResult }
  | { type: "feedbackTimeout" };

export const MAX_POLL_FAILURES = 3;

export function createInitialState(): AppState {
  return {
    ui: "LOADING",
    map: null,
    robot: null,
    pollFailures: 0,
    instanceChanged: false,
  };
}

export function reduce(state: AppState, event: AppEvent): AppState {
  switch (event.type) {
    case "loading":
      return { ...state, ui: "LOADING", instanceChanged: false };

    case "loaded":
      return {
        ...state,
        ui: "READY",
        map: event.map,
        robot: event.robot,
        pollFailures: 0,
        instanceChanged: false,
      };

    case "pollOk": {
      const prev = state.robot;
      // 服务重启：instanceId 变化，进入 LOADING 并触发重载。
      if (prev && event.robot.instanceId !== prev.instanceId) {
        return { ...state, ui: "LOADING", robot: null, instanceChanged: true };
      }
      // 同实例但序列号更小：忽略过期状态。
      if (prev && event.robot.sequence < prev.sequence) {
        return state;
      }
      // 命令执行中收到轮询：只更新数据，保持 EXECUTING 以免按钮提前放开。
      const ui: UiState = state.ui === "EXECUTING" ? "EXECUTING" : "READY";
      return { ...state, ui, robot: event.robot, pollFailures: 0 };
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
      return { ...state, ui, robot: event.robot, pollFailures: 0 };
    }

    case "feedbackTimeout":
      return { ...state, ui: "READY" };
  }
}
