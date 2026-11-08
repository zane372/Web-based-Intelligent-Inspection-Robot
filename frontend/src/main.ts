// 入口：把状态机、Canvas 渲染、API 客户端和 DOM 控件编排在一起。
import "./styles.css";
import { fetchMap, fetchRobot, sendCommand } from "./api/client";
import { DEFAULT_CELL_SIZE, draw, setupCanvas } from "./canvas/renderer";
import { createInitialState, reduce } from "./state";
import type { AppEvent, AppState } from "./state";
import type { CommandType, Heading } from "./types";

const POLL_INTERVAL_MS = 1000;
const BOUNDARY_FEEDBACK_MS = 1500;
const LOAD_RETRY_COUNT = 3;
const LOAD_RETRY_DELAY_MS = 1000;

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

// DOM 元素在 index.html 中静态声明；若缺失属配置错误，立即暴露。
const elements = [
  canvas, statusEl, positionEl, headingEl, lastResultEl, sequenceEl, motionStatusEl,
  btnUp, btnDown, btnLeft, btnRight, btnReset, btnReconnect, btnPatrol,
];
for (const el of elements) {
  if (!el) {
    throw new Error("缺少必要 DOM 元素，请检查 index.html");
  }
}

let state: AppState = createInitialState();
let ctx: CanvasRenderingContext2D | null = null;
let pollTimer: number | undefined;
let feedbackTimer: number | undefined;

function dispatch(event: AppEvent): void {
  state = reduce(state, event);
  if (state.instanceChanged) {
    void loadAll();
  }
  render();
  syncPolling();
}

function statusText(s: AppState): string {
  if (s.robot?.patrolActive) {
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
    }
  }
}

// 加载地图与初始状态；失败自动重试，超过次数后进入 DISCONNECTED。
async function loadAll(): Promise<void> {
  dispatch({ type: "loading" });
  for (let attempt = 1; attempt <= LOAD_RETRY_COUNT; attempt++) {
    try {
      const [map, robot] = await Promise.all([fetchMap(), fetchRobot()]);
      ctx = null; // 地图可能变化，强制重新初始化画布。
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
}

async function pollOnce(): Promise<void> {
  try {
    const robot = await fetchRobot();
    dispatch({ type: "pollOk", robot });
  } catch {
    dispatch({ type: "pollFail" });
  }
}

// 只在可见且非 LOADING/DISCONNECTED 时轮询。
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
    // POST 不自动重试：超时/失败先 GET 重新同步状态。
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
    syncPolling(); // 页面隐藏时暂停轮询。
  } else {
    void pollOnce(); // 页面可见时立即查询一次。
    syncPolling();
  }
});

void loadAll();
