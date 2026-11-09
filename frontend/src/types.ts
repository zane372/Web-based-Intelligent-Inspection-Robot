// 与后端 API 契约一一对应的 TypeScript 类型（见 SPEC 4.2）。
// 标准版扩展：障碍物 + 自动巡检。

export type Heading = "UP" | "DOWN" | "LEFT" | "RIGHT";
export type MotionStatus = "IDLE" | "BLOCKED";
export type LastResult =
  | "NONE"
  | "MOVED"
  | "BLOCKED_BOUNDARY"
  | "BLOCKED_OBSTACLE"
  | "RESET"
  | "PATROL_STARTED"
  | "PATROL_STOPPED"
  | "PATROL_COMPLETED";
export type CommandType = "MOVE" | "RESET" | "START_PATROL" | "STOP_PATROL";
export type CommandResult =
  | "MOVED"
  | "BLOCKED_BOUNDARY"
  | "BLOCKED_OBSTACLE"
  | "RESET"
  | "PATROL_STARTED"
  | "PATROL_STOPPED";

export interface Position {
  x: number;
  y: number;
}

export interface MapInfo {
  mapId: string;
  width: number;
  height: number;
  origin: "TOP_LEFT";
  xAxis: "RIGHT";
  yAxis: "DOWN";
  obstacles: Position[];
}

export interface RobotState {
  robotId: string;
  instanceId: string;
  position: Position;
  heading: Heading;
  motionStatus: MotionStatus;
  lastResult: LastResult;
  sequence: number;
  patrolActive: boolean;
}

export interface CommandResponse {
  commandId: string;
  type: CommandType;
  result: CommandResult;
  robot: RobotState;
}

// ---- 虚拟感知（SPEC 6）----
export type HitType = "NONE" | "BOUNDARY" | "OBSTACLE";
export type RelativeDirection = "FRONT" | "LEFT" | "RIGHT";
export type HazardType = "SMOKE" | "HIGH_TEMP" | "GAS_LEAK";
export type Severity = "LOW" | "MEDIUM" | "HIGH";
export type PerceptionEventType =
  | "ANOMALY_DETECTED"
  | "ANOMALY_OUT_OF_RANGE"
  | "CONTACT_BOUNDARY"
  | "CONTACT_OBSTACLE";

export interface RangeReading {
  sensorId: "range-front" | "range-left" | "range-right";
  relativeDirection: RelativeDirection;
  absoluteDirection: Heading;
  hitType: HitType;
  distanceCells: number | null;
  cell: Position | null;
}

export interface Detection {
  hazardId: string;
  hazardType: HazardType;
  severity: Severity;
  label: string | null;
  position: Position;
  distanceCells: number;
}

export interface PerceptionObservation {
  sequence: number;
  rangeCells: number;
  ranges: RangeReading[];
  detections: Detection[];
}

export interface PerceptionEvent {
  eventId: number;
  instanceId: string;
  sequence: number;
  type: PerceptionEventType;
  position: Position;
  hazard: Detection | null;
  contactResult: "BLOCKED_BOUNDARY" | "BLOCKED_OBSTACLE" | null;
}

export interface PerceptionLatest {
  enabled: boolean;
  instanceId: string;
  observation: PerceptionObservation | null;
  activeDetections: Detection[];
  latestEventId: number;
}

export interface PerceptionEvents {
  instanceId: string;
  events: PerceptionEvent[];
  oldestEventId: number | null;
  latestEventId: number;
  hasMore: boolean;
  truncated: boolean;
}
