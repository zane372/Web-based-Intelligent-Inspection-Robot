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
