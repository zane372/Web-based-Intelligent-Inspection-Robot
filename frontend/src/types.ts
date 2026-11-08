// 与后端 API 契约一一对应的 TypeScript 类型（见 SPEC 4.2）。

export type Heading = "UP" | "DOWN" | "LEFT" | "RIGHT";
export type MotionStatus = "IDLE" | "BLOCKED";
export type LastResult = "NONE" | "MOVED" | "BLOCKED_BOUNDARY" | "RESET";
export type CommandType = "MOVE" | "RESET";
export type CommandResult = "MOVED" | "BLOCKED_BOUNDARY" | "RESET";

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
}

export interface RobotState {
  robotId: string;
  instanceId: string;
  position: Position;
  heading: Heading;
  motionStatus: MotionStatus;
  lastResult: LastResult;
  sequence: number;
}

export interface CommandResponse {
  commandId: string;
  type: CommandType;
  result: CommandResult;
  robot: RobotState;
}
