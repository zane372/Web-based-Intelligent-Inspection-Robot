// 机器人控制编排：把纯 reducer（state.ts）与副作用（fetch、定时器、轮询）
// 连接起来。仅此层持有副作用，reducer 保持纯函数可单测。
import { useCallback, useEffect, useRef, useState } from "react";
import type { Dispatch } from "react";
import { fetchMap, fetchRobot, sendCommand } from "../api/client";
import type { AppEvent, AppState } from "../state";
import type { CommandType, Heading } from "../types";

const POLL_INTERVAL_MS = 1000;
const BOUNDARY_FEEDBACK_MS = 1500;
const PATROL_ALERT_MS = 1500;
const LOAD_RETRY_COUNT = 3;
const LOAD_RETRY_DELAY_MS = 1000;

export interface RobotController {
  move: (direction: Heading) => void;
  reset: () => void;
  togglePatrol: () => void;
  reconnect: () => void;
}

export function useRobotController(
  state: AppState,
  dispatch: Dispatch<AppEvent>,
): RobotController {
  const feedbackTimer = useRef<number | undefined>(undefined);
  const patrolAlertTimer = useRef<number | undefined>(undefined);
  const pollInFlight = useRef(false);
  const loadInFlight = useRef(false);
  const [visible, setVisible] = useState(
    () => document.visibilityState === "visible",
  );

  const loadAll = useCallback(async (): Promise<void> => {
    if (loadInFlight.current) {
      return;
    }
    loadInFlight.current = true;
    try {
      dispatch({ type: "loading" });
      for (let attempt = 1; attempt <= LOAD_RETRY_COUNT; attempt++) {
        try {
          const [map, robot] = await Promise.all([fetchMap(), fetchRobot()]);
          dispatch({ type: "loaded", map, robot });
          return;
        } catch {
          if (attempt < LOAD_RETRY_COUNT) {
            await new Promise<void>((resolve) =>
              setTimeout(resolve, LOAD_RETRY_DELAY_MS),
            );
          }
        }
      }
      dispatch({ type: "disconnect" });
    } finally {
      loadInFlight.current = false;
    }
  }, [dispatch]);

  const pollOnce = useCallback(async (): Promise<void> => {
    if (pollInFlight.current) {
      return;
    }
    pollInFlight.current = true;
    try {
      const robot = await fetchRobot();
      dispatch({ type: "pollOk", robot });
    } catch {
      dispatch({ type: "pollFail" });
    } finally {
      pollInFlight.current = false;
    }
  }, [dispatch]);

  const onCommand = useCallback(
    async (type: CommandType, direction?: Heading): Promise<void> => {
      if (state.ui !== "READY" && state.ui !== "BOUNDARY_FEEDBACK") {
        return;
      }
      if (feedbackTimer.current !== undefined) {
        window.clearTimeout(feedbackTimer.current);
        feedbackTimer.current = undefined;
      }
      try {
        dispatch({ type: "commandStart" });
        const resp = await sendCommand(type, direction);
        dispatch({ type: "commandDone", robot: resp.robot, result: resp.result });
        if (
          resp.result === "BLOCKED_BOUNDARY" ||
          resp.result === "BLOCKED_OBSTACLE"
        ) {
          feedbackTimer.current = window.setTimeout(() => {
            feedbackTimer.current = undefined;
            dispatch({ type: "feedbackTimeout" });
          }, BOUNDARY_FEEDBACK_MS);
        }
      } catch {
        try {
          const robot = await fetchRobot();
          dispatch({ type: "commandFailed", robot });
        } catch {
          dispatch({ type: "pollFail" });
        }
      }
    },
    [state.ui, dispatch],
  );

  const move = useCallback(
    (direction: Heading) => void onCommand("MOVE", direction),
    [onCommand],
  );
  const reset = useCallback(() => void onCommand("RESET"), [onCommand]);
  const togglePatrol = useCallback(
    () =>
      void onCommand(
        state.robot?.patrolActive ? "STOP_PATROL" : "START_PATROL",
      ),
    [onCommand, state.robot?.patrolActive],
  );
  const reconnect = useCallback(() => void loadAll(), [loadAll]);

  // 首次挂载加载。
  useEffect(() => {
    void loadAll();
  }, [loadAll]);

  // 服务重启（instanceId 变化）触发重载。
  useEffect(() => {
    if (state.instanceChanged) {
      void loadAll();
    }
  }, [state.instanceChanged, loadAll]);

  // 进入 LOADING 后清除旧的边界反馈定时器。
  useEffect(() => {
    if (state.ui === "LOADING" && feedbackTimer.current !== undefined) {
      window.clearTimeout(feedbackTimer.current);
      feedbackTimer.current = undefined;
    }
  }, [state.ui]);

  // 巡检报警锁存的定时器：报警态保持 PATROL_ALERT_MS 后清除。
  useEffect(() => {
    if (state.patrolAlert) {
      if (patrolAlertTimer.current === undefined) {
        patrolAlertTimer.current = window.setTimeout(() => {
          patrolAlertTimer.current = undefined;
          dispatch({ type: "patrolAlertTimeout" });
        }, PATROL_ALERT_MS);
      }
    } else if (patrolAlertTimer.current !== undefined) {
      window.clearTimeout(patrolAlertTimer.current);
      patrolAlertTimer.current = undefined;
    }
  }, [state.patrolAlert, dispatch]);

  // 可见性监听。
  useEffect(() => {
    const onVis = () => setVisible(document.visibilityState === "visible");
    document.addEventListener("visibilitychange", onVis);
    return () => document.removeEventListener("visibilitychange", onVis);
  }, []);

  // 轮询：可见且非 LOADING/DISCONNECTED 时启动 1s 间隔。
  useEffect(() => {
    const shouldPoll =
      visible && state.ui !== "LOADING" && state.ui !== "DISCONNECTED";
    if (!shouldPoll) {
      return;
    }
    const id = window.setInterval(() => void pollOnce(), POLL_INTERVAL_MS);
    return () => window.clearInterval(id);
  }, [visible, state.ui, pollOnce]);

  // 重新可见时立即补拉一次。
  useEffect(() => {
    if (!visible) {
      return;
    }
    if (state.ui !== "LOADING" && state.ui !== "DISCONNECTED") {
      void pollOnce();
    }
    // 仅在「从隐藏变为可见」的瞬间补拉，避免依赖其它状态反复触发。
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [visible]);

  // 卸载清理。
  useEffect(() => {
    return () => {
      if (feedbackTimer.current !== undefined) {
        window.clearTimeout(feedbackTimer.current);
      }
      if (patrolAlertTimer.current !== undefined) {
        window.clearTimeout(patrolAlertTimer.current);
      }
    };
  }, []);

  return { move, reset, togglePatrol, reconnect };
}
