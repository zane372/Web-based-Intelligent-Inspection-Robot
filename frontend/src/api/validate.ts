// 运行时字段级校验：在「后端畸形响应」进入状态机/渲染层之前拦截。
// request<T> 只做顶层 object 检查，`{}` / 缺字段对象仍会被静默 cast 成 T，
// 随后在 render 里访问 robot.position.x 等抛 TypeError（被轮询误判为失败、污染状态）。
// 这里为每个 API 响应类型提供 type guard，由 client.ts 在请求成功后调用。
import type {
  CommandResponse,
  Detection,
  Heading,
  MapInfo,
  PerceptionEvent,
  PerceptionEvents,
  PerceptionLatest,
  PerceptionObservation,
  Position,
  RangeReading,
  RobotState,
} from "../types";

function isObject(v: unknown): v is Record<string, unknown> {
  return typeof v === "object" && v !== null && !Array.isArray(v);
}
function isString(v: unknown): v is string {
  return typeof v === "string";
}
function isNumber(v: unknown): v is number {
  return typeof v === "number" && Number.isFinite(v);
}
function isBoolean(v: unknown): v is boolean {
  return typeof v === "boolean";
}
function isOneOf<T extends string>(v: unknown, allowed: readonly T[]): v is T {
  return isString(v) && (allowed as readonly string[]).includes(v);
}

const HEADINGS = ["UP", "DOWN", "LEFT", "RIGHT"] as const;
const MOTION_STATUSES = ["IDLE", "BLOCKED"] as const;
const LAST_RESULTS = [
  "NONE",
  "MOVED",
  "BLOCKED_BOUNDARY",
  "BLOCKED_OBSTACLE",
  "RESET",
  "PATROL_STARTED",
  "PATROL_STOPPED",
  "PATROL_COMPLETED",
] as const;
const COMMAND_TYPES = ["MOVE", "RESET", "START_PATROL", "STOP_PATROL"] as const;
const COMMAND_RESULTS = [
  "MOVED",
  "BLOCKED_BOUNDARY",
  "BLOCKED_OBSTACLE",
  "RESET",
  "PATROL_STARTED",
  "PATROL_STOPPED",
] as const;
const HIT_TYPES = ["NONE", "BOUNDARY", "OBSTACLE"] as const;
const RELATIVE_DIRECTIONS = ["FRONT", "LEFT", "RIGHT"] as const;
const HAZARD_TYPES = ["SMOKE", "HIGH_TEMP", "GAS_LEAK", "OIL_LEAK"] as const;
const SEVERITIES = ["LOW", "MEDIUM", "HIGH"] as const;
const EVENT_TYPES = [
  "ANOMALY_DETECTED",
  "ANOMALY_OUT_OF_RANGE",
  "CONTACT_BOUNDARY",
  "CONTACT_OBSTACLE",
] as const;
const CONTACT_RESULTS = ["BLOCKED_BOUNDARY", "BLOCKED_OBSTACLE"] as const;
const SENSOR_IDS = ["range-front", "range-left", "range-right"] as const;

function isHeading(v: unknown): v is Heading {
  return isOneOf(v, HEADINGS);
}
function isPosition(v: unknown): v is Position {
  return isObject(v) && isNumber(v.x) && isNumber(v.y);
}
function isDetection(v: unknown): v is Detection {
  return (
    isObject(v) &&
    isString(v.hazardId) &&
    isOneOf(v.hazardType, HAZARD_TYPES) &&
    isOneOf(v.severity, SEVERITIES) &&
    (v.label === null || isString(v.label)) &&
    isPosition(v.position) &&
    isNumber(v.distanceCells)
  );
}
function isRangeReading(v: unknown): v is RangeReading {
  return (
    isObject(v) &&
    isOneOf(v.sensorId, SENSOR_IDS) &&
    isOneOf(v.relativeDirection, RELATIVE_DIRECTIONS) &&
    isHeading(v.absoluteDirection) &&
    isOneOf(v.hitType, HIT_TYPES) &&
    (v.distanceCells === null || isNumber(v.distanceCells)) &&
    (v.cell === null || isPosition(v.cell))
  );
}
function isPerceptionObservation(v: unknown): v is PerceptionObservation {
  return (
    isObject(v) &&
    isNumber(v.sequence) &&
    isNumber(v.rangeCells) &&
    Array.isArray(v.ranges) &&
    v.ranges.every(isRangeReading) &&
    Array.isArray(v.detections) &&
    v.detections.every(isDetection)
  );
}
function isPerceptionEvent(v: unknown): v is PerceptionEvent {
  return (
    isObject(v) &&
    isNumber(v.eventId) &&
    isString(v.instanceId) &&
    isNumber(v.sequence) &&
    isOneOf(v.type, EVENT_TYPES) &&
    isPosition(v.position) &&
    (v.hazard === undefined ||
      v.hazard === null ||
      isDetection(v.hazard)) &&
    (v.contactResult === undefined ||
      v.contactResult === null ||
      isOneOf(v.contactResult, CONTACT_RESULTS))
  );
}

export function isRobotState(v: unknown): v is RobotState {
  return (
    isObject(v) &&
    isString(v.robotId) &&
    isString(v.instanceId) &&
    isPosition(v.position) &&
    isHeading(v.heading) &&
    isOneOf(v.motionStatus, MOTION_STATUSES) &&
    isOneOf(v.lastResult, LAST_RESULTS) &&
    isNumber(v.sequence) &&
    isBoolean(v.patrolActive)
  );
}

export function isMapInfo(v: unknown): v is MapInfo {
  return (
    isObject(v) &&
    isString(v.mapId) &&
    isNumber(v.width) &&
    isNumber(v.height) &&
    v.origin === "TOP_LEFT" &&
    v.xAxis === "RIGHT" &&
    v.yAxis === "DOWN" &&
    Array.isArray(v.obstacles) &&
    v.obstacles.every(isPosition)
  );
}

export function isCommandResponse(v: unknown): v is CommandResponse {
  return (
    isObject(v) &&
    isString(v.commandId) &&
    isOneOf(v.type, COMMAND_TYPES) &&
    isOneOf(v.result, COMMAND_RESULTS) &&
    isRobotState(v.robot)
  );
}

export function isPerceptionLatest(v: unknown): v is PerceptionLatest {
  return (
    isObject(v) &&
    isBoolean(v.enabled) &&
    isString(v.instanceId) &&
    (v.observation === null || isPerceptionObservation(v.observation)) &&
    Array.isArray(v.activeDetections) &&
    v.activeDetections.every(isDetection) &&
    isNumber(v.latestEventId)
  );
}

export function isPerceptionEvents(v: unknown): v is PerceptionEvents {
  return (
    isObject(v) &&
    isString(v.instanceId) &&
    Array.isArray(v.events) &&
    v.events.every(isPerceptionEvent) &&
    (v.oldestEventId === null || isNumber(v.oldestEventId)) &&
    isNumber(v.latestEventId) &&
    isBoolean(v.hasMore) &&
    isBoolean(v.truncated)
  );
}
