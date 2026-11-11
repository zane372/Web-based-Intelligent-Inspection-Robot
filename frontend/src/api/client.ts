// API 客户端：基于 fetch 的薄封装，只发相对路径 /api/v1/...。
// 生产环境由后端同源托管；本地开发由 Vite 代理到后端。
import type {
  CommandResponse,
  CommandType,
  Heading,
  MapInfo,
  PerceptionEvents,
  PerceptionLatest,
  RobotState,
} from "../types";
import {
  isCommandResponse,
  isMapInfo,
  isPerceptionEvents,
  isPerceptionLatest,
  isRobotState,
} from "./validate";

const BASE_PATH = "/api/v1";
const TIMEOUT_MS = 5000;

// 后端统一错误响应（SPEC 4.2.9）。
interface ErrorBody {
  error?: {
    code?: string;
    message?: string;
    requestId?: string;
  };
}

function asErrorBody(body: unknown): ErrorBody | null {
  if (typeof body === "object" && body !== null) {
    return body as ErrorBody;
  }
  return null;
}

export class ApiError extends Error {
  readonly code: string;
  readonly requestId?: string;

  constructor(code: string, message: string, requestId?: string) {
    super(message);
    this.name = "ApiError";
    this.code = code;
    this.requestId = requestId;
  }
}

async function request<T>(
  path: string,
  init?: RequestInit,
  validate?: (value: unknown) => value is T,
): Promise<T> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), TIMEOUT_MS);
  try {
    const response = await fetch(`${BASE_PATH}${path}`, {
      ...init,
      signal: controller.signal,
    });
    const text = await response.text();
    let body: unknown = null;
    if (text.length > 0) {
      try {
        body = JSON.parse(text);
      } catch {
        body = null;
      }
    }
    if (!response.ok) {
      const err = asErrorBody(body)?.error;
      throw new ApiError(
        err?.code ?? "INTERNAL_ERROR",
        err?.message ?? `HTTP ${response.status}`,
        err?.requestId,
      );
    }
    // 成功响应必须是 JSON 对象（而非 null/数组/原始值），否则视为后端异常，
    // 避免把畸形响应静默 cast 成 T 后在渲染层崩溃。
    if (typeof body !== "object" || body === null || Array.isArray(body)) {
      throw new ApiError(
        "INVALID_RESPONSE",
        `HTTP ${response.status} 响应体不是 JSON 对象`,
      );
    }
    // 字段级校验：畸形对象（{} / 缺字段 / 类型错误）在进入状态机前拦截，
    // 避免 body as T 静默 cast 后在渲染层抛 TypeError。
    if (validate && !validate(body)) {
      throw new ApiError(
        "INVALID_RESPONSE",
        `HTTP ${response.status} 响应体字段缺失或类型错误`,
      );
    }
    return body as T;
  } finally {
    clearTimeout(timer);
  }
}

export function fetchMap(): Promise<MapInfo> {
  return request<MapInfo>("/map", undefined, isMapInfo);
}

export function fetchRobot(): Promise<RobotState> {
  return request<RobotState>("/robot", undefined, isRobotState);
}

export function sendCommand(
  type: CommandType,
  direction?: Heading,
): Promise<CommandResponse> {
  const payload = direction ? { type, direction } : { type };
  return request<CommandResponse>(
    "/robot/commands",
    {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    },
    isCommandResponse,
  );
}

export function fetchPerceptionLatest(): Promise<PerceptionLatest> {
  return request<PerceptionLatest>(
    "/perception/latest",
    undefined,
    isPerceptionLatest,
  );
}

export function fetchPerceptionEvents(
  after: number,
  limit: number,
): Promise<PerceptionEvents> {
  return request<PerceptionEvents>(
    `/perception/events?after=${after}&limit=${limit}`,
    undefined,
    isPerceptionEvents,
  );
}
