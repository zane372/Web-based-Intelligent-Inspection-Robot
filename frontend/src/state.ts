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
  | { type: "commandFailed"; robot: RobotState }
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
      // 保留旧快照（不置 null），否则后续过期响应会因 prev 为空而跳过
      // instanceId/sequence 校验，把 LOADING 提前打回 READY 并写入旧数据。
      if (prev && event.robot.instanceId !== prev.instanceId) {
        return {
          ...state,
          ui: "LOADING",
          instanceChanged: true,
          patrolAlert: null,
        };
      }
      // 同实例但序列号更小：忽略过期状态。
      if (prev && event.robot.sequence < prev.sequence) {
        return state;
      }
      // 命令执行中、边界/障碍反馈展示期间或重载（LOADING）期间收到轮询：
      // 只更新数据，保持当前 UI 态以免按钮提前放开或反馈文案被 1s 轮询秒覆盖
      // （反馈由 1.5s 定时器收尾）；LOADING 期间旧实例的过期轮询同样不得打回 READY。
      const ui: UiState =
        state.ui === "EXECUTING" ||
        state.ui === "BOUNDARY_FEEDBACK" ||
        state.ui === "LOADING"
          ? state.ui
          : "READY";
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
      // 重载期间或命令响应来自不同实例（服务重启）：一律忽略。LOADING 的唯一
      // 正道是 loaded；实例变化由 pollOk 检测并触发重载，在途命令响应不得把
      // LOADING/当前态覆盖回 BOUNDARY_FEEDBACK 或旧实例数据。
      if (
        state.ui === "LOADING" ||
        (prev && event.robot.instanceId !== prev.instanceId)
      ) {
        return state;
      }
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

    case "commandFailed": {
      const prev = state.robot;
      // 服务重启：instanceId 变化，进入 LOADING 并触发重载（与 pollOk 一致）。
      // 同样保留旧快照，防过期响应打回 READY。
      if (prev && event.robot.instanceId !== prev.instanceId) {
        return {
          ...state,
          ui: "LOADING",
          instanceChanged: true,
          patrolAlert: null,
        };
      }
      // 命令发送失败但服务仍可达（fetchRobot 成功）：回到 READY 并刷新快照，
      // 避免命令失败后 pollOk 把 UI 永久锁死在 EXECUTING。
      // 若恢复快照比当前更旧（sequence 更小，来自迟到的并发轮询/多副本），保留
      // 当前更新的快照不回写，但仍要退出 EXECUTING 回到 READY（否则卡死）。
      const robot =
        prev && event.robot.sequence < prev.sequence ? prev : event.robot;
      return {
        ...state,
        ui: "READY",
        robot,
        pollFailures: 0,
        patrolAlert: patrolAlertFor(robot, state.patrolAlert),
      };
    }

    case "feedbackTimeout":
      // 仅 BOUNDARY_FEEDBACK 展示期结束才收尾到 READY；其它态（LOADING/DEGRADED/
      // DISCONNECTED/EXECUTING）保持不变，防止 1.5s 定时器在服务重启或断连窗口内
      // 把状态错误拉回 READY（按钮提前放开）。
      return state.ui === "BOUNDARY_FEEDBACK" ? { ...state, ui: "READY" } : state;

    case "patrolAlertTimeout":
      return { ...state, patrolAlert: null };
  }
}
