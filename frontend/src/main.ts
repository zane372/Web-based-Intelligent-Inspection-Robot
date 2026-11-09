// 入口：把机器人状态机、感知状态机、Canvas 渲染、API 客户端和 DOM 控件
// 编排在一起。机器人 UI 与感知面板各自独立的状态机（SPEC 6.8），互不
// 干扰：感知故障不得断连手动/巡检，反之亦然。
import "./styles.css";
import {
  fetchMap,
  fetchPerceptionEvents,
  fetchPerceptionLatest,
  fetchRobot,
  sendCommand,
} from "./api/client";
import {
  DEFAULT_CELL_SIZE,
  draw,
  drawPerceptionLayer,
  setupCanvas,
} from "./canvas/renderer";
import {
  createInitialPerceptionState,
  EVENTS_LIMIT,
  reducePerception,
} from "./perception_state";
import type { PerceptionAction, PerceptionState } from "./perception_state";
import { createInitialState, reduce } from "./state";
import type { AppEvent, AppState } from "./state";
import type {
  CommandType,
  Detection,
  Heading,
  HitType,
  PerceptionEvent,
  RangeReading,
  Severity,
} from "./types";

const POLL_INTERVAL_MS = 1000;
const BOUNDARY_FEEDBACK_MS = 1500;
const PATROL_ALERT_MS = 1500;
const LOAD_RETRY_COUNT = 3;
const LOAD_RETRY_DELAY_MS = 1000;

// ---- 机器人 DOM ----
const canvas = document.querySelector<HTMLCanvasElement>("#map-canvas");
const statusEl = document.querySelector<HTMLParagraphElement>("#status");
const positionEl = document.querySelector<HTMLElement>("#readout-position");
const headingEl = document.querySelector<HTMLElement>("#readout-heading");
const lastResultEl = document.querySelector<HTMLElement>("#readout-last-result");
const sequenceEl = document.querySelector<HTMLElement>("#readout-sequence");
const motionStatusEl = document.querySelector<HTMLElement>("#readout-motion-status");

const btnUp = document.querySelector<HTMLButtonElement>("#btn-up");
const btnDown = document.querySelector<HTMLButtonElement>("#btn-down");
const btnLeft = document.querySelector<HTMLButtonElement>("#btn-left");
const btnRight = document.querySelector<HTMLButtonElement>("#btn-right");
const btnReset = document.querySelector<HTMLButtonElement>("#btn-reset");
const btnReconnect = document.querySelector<HTMLButtonElement>("#btn-reconnect");
const btnPatrol = document.querySelector<HTMLButtonElement>("#btn-patrol");

// ---- 感知 DOM ----
const perceptionStatusEl =
  document.querySelector<HTMLParagraphElement>("#perception-status");
const rangeFrontEl = document.querySelector<HTMLElement>("#range-front");
const rangeLeftEl = document.querySelector<HTMLElement>("#range-left");
const rangeRightEl = document.querySelector<HTMLElement>("#range-right");
const perceptionSequenceEl =
  document.querySelector<HTMLElement>("#perception-sequence");
const anomalyListEl =
  document.querySelector<HTMLUListElement>("#perception-anomaly-list");
const eventListEl =
  document.querySelector<HTMLUListElement>("#perception-event-list");

// DOM 元素在 index.html 中静态声明；若缺失属配置错误，立即暴露。
const elements = [
  canvas, statusEl, positionEl, headingEl, lastResultEl, sequenceEl,
  motionStatusEl, btnUp, btnDown, btnLeft, btnRight, btnReset, btnReconnect,
  btnPatrol, perceptionStatusEl, rangeFrontEl, rangeLeftEl, rangeRightEl,
  perceptionSequenceEl, anomalyListEl, eventListEl,
];
for (const el of elements) {
  if (!el) {
    throw new Error("缺少必要 DOM 元素，请检查 index.html");
  }
}

// ---- 机器人状态机 ----
let state: AppState = createInitialState();
let ctx: CanvasRenderingContext2D | null = null;
let pollTimer: number | undefined;
let feedbackTimer: number | undefined;
let patrolAlertTimer: number | undefined;
let pollInFlight = false;
let loadInFlight = false;

// ---- 感知状态机（独立）----
let perceptionState: PerceptionState = createInitialPerceptionState();
let perceptionPollTimer: number | undefined;
let perceptionPollInFlight = false;
let perceptionEventsInFlight = false;

function dispatch(event: AppEvent): void {
  state = reduce(state, event);
  if (state.patrolAlert && patrolAlertTimer === undefined) {
    patrolAlertTimer = window.setTimeout(() => {
      patrolAlertTimer = undefined;
      dispatch({ type: "patrolAlertTimeout" });
    }, PATROL_ALERT_MS);
  } else if (!state.patrolAlert && patrolAlertTimer !== undefined) {
    window.clearTimeout(patrolAlertTimer);
    patrolAlertTimer = undefined;
  }
  if (state.instanceChanged) {
    void loadAll();
  }
  render();
  syncPolling();
}

function dispatchPerception(action: PerceptionAction): void {
  perceptionState = reducePerception(perceptionState, action);
  renderPerception();
  syncPerceptionPolling();
}

function statusText(s: AppState): string {
  if (
    (s.ui === "READY" || s.ui === "EXECUTING") &&
    s.robot?.patrolActive &&
    s.patrolAlert !== null
  ) {
    return s.patrolAlert === "BLOCKED_OBSTACLE"
      ? "巡检碰到障碍物，正在绕行"
      : "巡检到达边界，正在绕行";
  }
  if ((s.ui === "READY" || s.ui === "EXECUTING") && s.robot?.patrolActive) {
    return "自动巡检中…";
  }
  switch (s.ui) {
    case "LOADING":
      return "加载中…";
    case "READY":
      return "就绪";
    case "EXECUTING":
      return "执行中…";
    case "BOUNDARY_FEEDBACK":
      return s.robot?.lastResult === "BLOCKED_OBSTACLE"
        ? "前方有障碍物，无法前进"
        : "已到达边界，无法继续前进";
    case "DEGRADED":
      return "连接不稳定，正在重试…";
    case "DISCONNECTED":
      return "连接已断开";
  }
}

function render(): void {
  statusEl!.textContent = statusText(state);
  statusEl!.dataset.state = state.ui;

  const robot = state.robot;
  positionEl!.textContent = robot ? `(${robot.position.x}, ${robot.position.y})` : "—";
  headingEl!.textContent = robot ? robot.heading : "—";
  lastResultEl!.textContent = robot ? robot.lastResult : "—";
  sequenceEl!.textContent = robot ? String(robot.sequence) : "—";
  motionStatusEl!.textContent = robot ? robot.motionStatus : "—";

  const canCommand = state.ui === "READY" || state.ui === "BOUNDARY_FEEDBACK";
  for (const btn of [btnUp, btnDown, btnLeft, btnRight, btnReset, btnPatrol]) {
    btn!.disabled = !canCommand;
  }
  btnPatrol!.textContent = state.robot?.patrolActive ? "停止巡检" : "自动巡检";
  btnReconnect!.hidden = state.ui !== "DISCONNECTED";

  if (state.map) {
    if (!ctx) {
      ctx = setupCanvas(canvas!, state.map, DEFAULT_CELL_SIZE);
    }
    if (ctx) {
      draw(ctx, state.map, state.robot, DEFAULT_CELL_SIZE);
      // 感知图层：只画已发现异常（SPEC 6.8）。
      drawPerceptionLayer(ctx, perceptionState.discovered, DEFAULT_CELL_SIZE);
    }
  }
}

// ---- 感知面板渲染 ----

function hitTypeText(hitType: HitType): string {
  switch (hitType) {
    case "NONE":
      return "无障碍";
    case "BOUNDARY":
      return "边界";
    case "OBSTACLE":
      return "障碍";
  }
}

function rangeText(r: RangeReading): string {
  if (r.hitType === "NONE" || r.distanceCells === null) {
    return hitTypeText(r.hitType);
  }
  return `${hitTypeText(r.hitType)} ${r.distanceCells} 格`;
}

function hazardTypeText(t: Detection["hazardType"]): string {
  switch (t) {
    case "SMOKE":
      return "烟雾";
    case "HIGH_TEMP":
      return "高温";
    case "GAS_LEAK":
      return "气体泄漏";
  }
}

function severityText(s: Severity): string {
  switch (s) {
    case "LOW":
      return "低";
    case "MEDIUM":
      return "中";
    case "HIGH":
      return "高";
  }
}

function severityClass(s: Severity): string {
  return `severity-${s.toLowerCase()}`;
}

function perceptionStatusText(s: PerceptionState): string {
  switch (s.ui) {
    case "DISABLED":
      return "感知已关闭";
    case "LOADING":
      return "感知加载中…";
    case "READY":
      return "感知就绪";
    case "DEGRADED":
      return "感知连接不稳定";
    case "DISCONNECTED":
      return "感知连接断开";
  }
}

function eventText(e: PerceptionEvent): string {
  const pos = `(${e.position.x},${e.position.y})`;
  switch (e.type) {
    case "ANOMALY_DETECTED":
      return `发现异常 ${hazardTypeText(e.hazard?.hazardType ?? "SMOKE")} @ ${pos}`;
    case "ANOMALY_OUT_OF_RANGE":
      return `异常离开范围 @ ${pos}`;
    case "CONTACT_BOUNDARY":
      return `接触边界 @ ${pos}`;
    case "CONTACT_OBSTACLE":
      return `接触障碍 @ ${pos}`;
  }
}

function renderPerception(): void {
  const s = perceptionState;
  perceptionStatusEl!.textContent = perceptionStatusText(s);
  perceptionStatusEl!.dataset.state = s.ui;

  const obs = s.observation;
  if (!obs) {
    rangeFrontEl!.textContent = "—";
    rangeLeftEl!.textContent = "—";
    rangeRightEl!.textContent = "—";
    perceptionSequenceEl!.textContent = "—";
  } else {
    const byRel = new Map(obs.ranges.map((r) => [r.relativeDirection, r]));
    rangeFrontEl!.textContent = rangeText(byRel.get("FRONT")!);
    rangeLeftEl!.textContent = rangeText(byRel.get("LEFT")!);
    rangeRightEl!.textContent = rangeText(byRel.get("RIGHT")!);
    perceptionSequenceEl!.textContent = String(obs.sequence);
  }

  // 活动异常（只显示已发现的，即当前半径内的活动检测）。
  anomalyListEl!.textContent = "";
  if (s.activeDetections.length === 0) {
    const li = document.createElement("li");
    li.textContent = "无";
    li.className = "perception-empty";
    anomalyListEl!.appendChild(li);
  } else {
    for (const d of s.activeDetections) {
      const li = document.createElement("li");
      const dot = document.createElement("span");
      dot.className = `anomaly-dot ${severityClass(d.severity)}`;
      dot.setAttribute("aria-hidden", "true");
      li.appendChild(dot);
      li.appendChild(
        document.createTextNode(
          `${hazardTypeText(d.hazardType)}（${severityText(d.severity)}）@ ` +
            `(${d.position.x},${d.position.y})，距离 ${d.distanceCells}`,
        ),
      );
      anomalyListEl!.appendChild(li);
    }
  }

  // 感知事件（按 eventId 去重，最多 20 条，aria-live polite）。
  eventListEl!.textContent = "";
  if (s.truncated) {
    const li = document.createElement("li");
    li.textContent = "事件已截断，正在重建…";
    li.className = "perception-empty";
    eventListEl!.appendChild(li);
  }
  for (const e of s.events) {
    const li = document.createElement("li");
    li.textContent = `#${e.eventId} ${eventText(e)}`;
    eventListEl!.appendChild(li);
  }
  if (s.events.length === 0 && !s.truncated) {
    const li = document.createElement("li");
    li.textContent = "暂无事件";
    li.className = "perception-empty";
    eventListEl!.appendChild(li);
  }
}

// ---- 加载与轮询 ----

async function loadAll(): Promise<void> {
  if (loadInFlight) {
    return;
  }
  loadInFlight = true;
  try {
    dispatch({ type: "loading" });
    for (let attempt = 1; attempt <= LOAD_RETRY_COUNT; attempt++) {
      try {
        const [map, robot] = await Promise.all([fetchMap(), fetchRobot()]);
        ctx = null;
        dispatch({ type: "loaded", map, robot });
        void pollPerceptionOnce();
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
    loadInFlight = false;
  }
}

async function pollOnce(): Promise<void> {
  if (pollInFlight) {
    return;
  }
  pollInFlight = true;
  try {
    const robot = await fetchRobot();
    dispatch({ type: "pollOk", robot });
  } catch {
    dispatch({ type: "pollFail" });
  } finally {
    pollInFlight = false;
  }
}

// 感知轮询：拉 latest + 增量拉事件（单飞，独立于机器人轮询）。
async function pollPerceptionOnce(): Promise<void> {
  if (perceptionPollInFlight) {
    return;
  }
  perceptionPollInFlight = true;
  try {
    const latest = await fetchPerceptionLatest();
    if (!latest.enabled) {
      dispatchPerception({ type: "disabled" });
      return;
    }
    dispatchPerception({ type: "pollOk", data: latest });
    await pollPerceptionEvents();
  } catch {
    dispatchPerception({ type: "pollFail" });
  } finally {
    perceptionPollInFlight = false;
  }
}

async function pollPerceptionEvents(): Promise<void> {
  if (perceptionEventsInFlight) {
    return;
  }
  perceptionEventsInFlight = true;
  try {
    // 增量分页拉取：after 游标随 reducer 推进，最多连续拉 10 页，
    // 避免事件积压时无限循环（审查 I1）。
    for (let page = 0; page < 10; page++) {
      const data = await fetchPerceptionEvents(
        perceptionState.after,
        EVENTS_LIMIT,
      );
      dispatchPerception({
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
    perceptionEventsInFlight = false;
  }
}

function syncPolling(): void {
  const shouldPoll =
    !document.hidden &&
    state.ui !== "LOADING" &&
    state.ui !== "DISCONNECTED";
  if (shouldPoll && pollTimer === undefined) {
    pollTimer = window.setInterval(() => void pollOnce(), POLL_INTERVAL_MS);
  } else if (!shouldPoll && pollTimer !== undefined) {
    window.clearInterval(pollTimer);
    pollTimer = undefined;
  }
}

function syncPerceptionPolling(): void {
  const shouldPoll =
    !document.hidden &&
    perceptionState.ui !== "DISABLED" &&
    perceptionState.ui !== "DISCONNECTED" &&
    perceptionState.ui !== "LOADING";
  if (shouldPoll && perceptionPollTimer === undefined) {
    perceptionPollTimer = window.setInterval(
      () => void pollPerceptionOnce(),
      POLL_INTERVAL_MS,
    );
  } else if (!shouldPoll && perceptionPollTimer !== undefined) {
    window.clearInterval(perceptionPollTimer);
    perceptionPollTimer = undefined;
  }
}

async function onCommand(type: CommandType, direction?: Heading): Promise<void> {
  if (state.ui !== "READY" && state.ui !== "BOUNDARY_FEEDBACK") {
    return;
  }
  if (feedbackTimer !== undefined) {
    window.clearTimeout(feedbackTimer);
    feedbackTimer = undefined;
  }
  dispatch({ type: "commandStart" });
  try {
    const resp = await sendCommand(type, direction);
    dispatch({ type: "commandDone", robot: resp.robot, result: resp.result });
    if (resp.result === "BLOCKED_BOUNDARY" || resp.result === "BLOCKED_OBSTACLE") {
      if (feedbackTimer !== undefined) {
        window.clearTimeout(feedbackTimer);
      }
      feedbackTimer = window.setTimeout(() => {
        dispatch({ type: "feedbackTimeout" });
      }, BOUNDARY_FEEDBACK_MS);
    }
  } catch {
    try {
      const robot = await fetchRobot();
      dispatch({ type: "pollOk", robot });
    } catch {
      dispatch({ type: "pollFail" });
    }
  }
}

btnUp!.addEventListener("click", () => void onCommand("MOVE", "UP"));
btnDown!.addEventListener("click", () => void onCommand("MOVE", "DOWN"));
btnLeft!.addEventListener("click", () => void onCommand("MOVE", "LEFT"));
btnRight!.addEventListener("click", () => void onCommand("MOVE", "RIGHT"));
btnReset!.addEventListener("click", () => void onCommand("RESET"));
btnReconnect!.addEventListener("click", () => void loadAll());
btnPatrol!.addEventListener("click", () => {
  void onCommand(state.robot?.patrolActive ? "STOP_PATROL" : "START_PATROL");
});

document.addEventListener("visibilitychange", () => {
  if (document.hidden) {
    syncPolling();
    syncPerceptionPolling();
  } else {
    void pollOnce();
    void pollPerceptionOnce();
    syncPolling();
    syncPerceptionPolling();
  }
});

void loadAll();
