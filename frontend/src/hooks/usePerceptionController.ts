// 感知面板独立编排：与机器人状态机解耦（SPEC 6.8），感知故障不得断连控制。
// after 游标由 ref 同步推进，避免 reducer 异步更新导致增量分页游标陈旧。
import { useCallback, useEffect, useRef, useState } from "react";
import type { Dispatch } from "react";
import { fetchPerceptionEvents, fetchPerceptionLatest } from "../api/client";
import {
  EVENTS_LIMIT,
  type PerceptionAction,
  type PerceptionState,
} from "../perception_state";

const POLL_INTERVAL_MS = 1000;
const MAX_EVENT_PAGES = 10;

export function usePerceptionController(
  state: PerceptionState,
  dispatch: Dispatch<PerceptionAction>,
): void {
  const pollInFlight = useRef(false);
  const eventsInFlight = useRef(false);
  const afterRef = useRef(0);
  const instanceIdRef = useRef<string | null>(null);
  const [visible, setVisible] = useState(
    () => document.visibilityState === "visible",
  );

  const pollPerceptionEvents = useCallback(async (): Promise<void> => {
    if (eventsInFlight.current) {
      return;
    }
    eventsInFlight.current = true;
    try {
      // 增量分页拉取：after 游标随返回事件的最大 eventId 推进，最多 10 页。
      for (let page = 0; page < MAX_EVENT_PAGES; page++) {
        const data = await fetchPerceptionEvents(afterRef.current, EVENTS_LIMIT);
        const maxEventId = data.events.reduce(
          (max, e) => Math.max(max, e.eventId),
          0,
        );
        if (data.truncated) {
          afterRef.current = 0; // 环形缓冲截断：游标归零重建。
        } else {
          afterRef.current =
            maxEventId > 0 ? maxEventId : afterRef.current;
        }
        dispatch({
          type: "eventsLoaded",
          data: {
            events: data.events,
            latestEventId: data.latestEventId,
            hasMore: data.hasMore,
            truncated: data.truncated,
          },
        });
        if (!data.hasMore || data.truncated) {
          break;
        }
      }
    } catch {
      // 事件拉取失败不单独降级：latest 仍成功即保持 READY，下轮重试。
    } finally {
      eventsInFlight.current = false;
    }
  }, [dispatch]);

  const pollPerceptionOnce = useCallback(async (): Promise<void> => {
    if (pollInFlight.current) {
      return;
    }
    pollInFlight.current = true;
    try {
      const latest = await fetchPerceptionLatest();
      if (!latest.enabled) {
        instanceIdRef.current = null;
        afterRef.current = 0;
        dispatch({ type: "disabled" });
        return;
      }
      if (
        instanceIdRef.current !== null &&
        instanceIdRef.current !== latest.instanceId
      ) {
        afterRef.current = 0; // 实例变化：从 0 重建。
      }
      instanceIdRef.current = latest.instanceId;
      dispatch({ type: "pollOk", data: latest });
      // 仅在事件编号前进时才增量拉取事件流，空闲时避免每秒空请求（audit P3）。
      if (latest.latestEventId > afterRef.current) {
        await pollPerceptionEvents();
      }
    } catch {
      dispatch({ type: "pollFail" });
    } finally {
      pollInFlight.current = false;
    }
  }, [dispatch, pollPerceptionEvents]);

  // 首次挂载拉取。
  useEffect(() => {
    void pollPerceptionOnce();
  }, [pollPerceptionOnce]);

  // 可见性监听。
  useEffect(() => {
    const onVis = () => setVisible(document.visibilityState === "visible");
    document.addEventListener("visibilitychange", onVis);
    return () => document.removeEventListener("visibilitychange", onVis);
  }, []);

  // 感知面板只读、无独立重连按钮：DISCONNECTED 也继续轮询以自动恢复；
  // 仅 DISABLED 与 LOADING 停轮询。
  useEffect(() => {
    const shouldPoll =
      visible && state.ui !== "DISABLED" && state.ui !== "LOADING";
    if (!shouldPoll) {
      return;
    }
    const id = window.setInterval(
      () => void pollPerceptionOnce(),
      POLL_INTERVAL_MS,
    );
    return () => window.clearInterval(id);
  }, [visible, state.ui, pollPerceptionOnce]);

  // 重新可见时立即补拉一次。
  useEffect(() => {
    if (!visible) {
      return;
    }
    if (state.ui !== "LOADING" && state.ui !== "DISABLED") {
      void pollPerceptionOnce();
    }
  }, [visible]);
}
