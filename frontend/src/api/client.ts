// API 客户端：基于 fetch 的薄封装，只发相对路径 /api/v1/...。
// 生产环境由后端同源托管；本地开发由 Vite 代理到后端。
import type {
  CommandResponse,
  CommandType,
  Heading,
  MapInfo,
  RobotState,
} from "../types";

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

async function request<T>(path: string, init?: RequestInit): Promise<T> {
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
    return body as T;
  } finally {
    clearTimeout(timer);
  }
}

export function fetchMap(): Promise<MapInfo> {
  return request<MapInfo>("/map");
}

export function fetchRobot(): Promise<RobotState> {
  return request<RobotState>("/robot");
}

export function sendCommand(
  type: CommandType,
  direction?: Heading,
): Promise<CommandResponse> {
  const payload = direction ? { type, direction } : { type };
  return request<CommandResponse>("/robot/commands", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
}
