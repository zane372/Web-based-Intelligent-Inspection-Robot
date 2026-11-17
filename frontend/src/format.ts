// 展示层纯函数：把后端枚举/状态翻译成中文文案与信号灯色调。
// 与状态机、渲染器解耦，便于单测与复用。
import type { PerceptionState } from "./perception_state";
import type { AppState } from "./state";
import type {
  Detection,
  Heading,
  HitType,
  LastResult,
  MotionStatus,
  PerceptionEvent,
  RangeReading,
  Severity,
} from "./types";

export function headingLabel(h: Heading): string {
  switch (h) {
    case "UP":
      return "上";
    case "DOWN":
      return "下";
    case "LEFT":
      return "左";
    case "RIGHT":
      return "右";
  }
}

export function motionStatusLabel(m: MotionStatus): string {
  return m === "IDLE" ? "静止" : "受阻";
}

export function lastResultLabel(r: LastResult): string {
  switch (r) {
    case "NONE":
      return "—";
    case "MOVED":
      return "已移动";
    case "BLOCKED_BOUNDARY":
      return "越界";
    case "BLOCKED_OBSTACLE":
      return "遇障";
    case "RESET":
      return "已重置";
    case "PATROL_STARTED":
      return "巡检启动";
    case "PATROL_STOPPED":
      return "巡检停止";
    case "PATROL_COMPLETED":
      return "巡检完成";
  }
}

export function statusText(s: AppState): string {
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

export function hitTypeText(h: HitType): string {
  switch (h) {
    case "NONE":
      return "无障碍";
    case "BOUNDARY":
      return "边界";
    case "OBSTACLE":
      return "障碍";
  }
}

export function rangeText(r: RangeReading | undefined): string {
  if (!r) {
    return "—";
  }
  if (r.hitType === "NONE" || r.distanceCells === null) {
    return hitTypeText(r.hitType);
  }
  return `${hitTypeText(r.hitType)} ${r.distanceCells} 格`;
}

export function hazardTypeText(t: Detection["hazardType"]): string {
  switch (t) {
    case "SMOKE":
      return "烟雾";
    case "HIGH_TEMP":
      return "高温";
    case "GAS_LEAK":
      return "气体泄漏";
    case "OIL_LEAK":
      return "油污泄漏";
  }
}

export function severityText(s: Severity): string {
  switch (s) {
    case "LOW":
      return "低";
    case "MEDIUM":
      return "中";
    case "HIGH":
      return "高";
  }
}

export function severityColor(s: Severity): string {
  switch (s) {
    case "LOW":
      return "#d97706";
    case "MEDIUM":
      return "#ea580c";
    case "HIGH":
      return "#bd2a22"; // 与 --color-danger 一致
  }
}

export function perceptionStatusText(s: PerceptionState): string {
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

export function eventText(e: PerceptionEvent): string {
  const pos = `(${e.position.x},${e.position.y})`;
  switch (e.type) {
    case "ANOMALY_DETECTED":
      return e.hazard
        ? `发现异常 ${hazardTypeText(e.hazard.hazardType)} @ ${pos}`
        : `发现异常（类型未知）@ ${pos}`;
    case "ANOMALY_OUT_OF_RANGE":
      return `异常离开范围 @ ${pos}`;
    case "CONTACT_BOUNDARY":
      return `接触边界 @ ${pos}`;
    case "CONTACT_OBSTACLE":
      return `接触障碍 @ ${pos}`;
  }
}

// 信号灯色调映射。
export type LampTone = "muted" | "green" | "amber" | "red";

export function lampClass(tone: LampTone, pulsing = false): string {
  return `signal-lamp is-${tone}${pulsing ? " is-pulsing" : ""}`;
}

export function robotLampTone(s: AppState): LampTone {
  if (s.robot?.patrolActive && s.patrolAlert !== null) {
    return "red";
  }
  if (s.robot?.patrolActive) {
    return "green";
  }
  switch (s.ui) {
    case "READY":
      return "green";
    case "EXECUTING":
      return "amber";
    case "BOUNDARY_FEEDBACK":
      return "red";
    case "DEGRADED":
      return "amber";
    case "DISCONNECTED":
      return "red";
    case "LOADING":
      return "muted";
  }
}

export function perceptionLampTone(s: PerceptionState): LampTone {
  switch (s.ui) {
    case "READY":
      return "green";
    case "DEGRADED":
      return "amber";
    case "DISCONNECTED":
      return "red";
    case "DISABLED":
      return "muted";
    case "LOADING":
      return "muted";
  }
}
