import { afterEach, describe, expect, it, vi } from "vitest";
import {
  ApiError,
  fetchPerceptionEvents,
  fetchPerceptionLatest,
  fetchRobot,
  sendCommand,
} from "../src/api/client";

const robotJson = {
  robotId: "robot-1",
  instanceId: "i1",
  position: { x: 0, y: 0 },
  heading: "RIGHT",
  motionStatus: "IDLE",
  lastResult: "NONE",
  sequence: 0,
};

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("API client", () => {
  it("fetchRobot 请求正确路径并解析机器人状态", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(JSON.stringify(robotJson), { status: 200 }),
    );
    vi.stubGlobal("fetch", mock);

    const robot = await fetchRobot();
    expect(robot.robotId).toBe("robot-1");
    expect(robot.heading).toBe("RIGHT");
    expect(mock).toHaveBeenCalledWith(
      "/api/v1/robot",
      expect.objectContaining({ signal: expect.anything() }),
    );
  });

  it("sendCommand 发送 JSON 命令体", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          commandId: "cmd-00000001",
          type: "MOVE",
          result: "MOVED",
          robot: { ...robotJson, sequence: 1 },
        }),
        { status: 201 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    const resp = await sendCommand("MOVE", "RIGHT");
    expect(resp.result).toBe("MOVED");
    expect(resp.robot.sequence).toBe(1);

    const [url, init] = mock.mock.calls[0] as [string, RequestInit];
    expect(url).toBe("/api/v1/robot/commands");
    expect(init.method).toBe("POST");
    expect(JSON.parse(init.body as string)).toEqual({
      type: "MOVE",
      direction: "RIGHT",
    });
  });

  it("RESET 命令不带 direction", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          commandId: "cmd-00000001",
          type: "RESET",
          result: "RESET",
          robot: robotJson,
        }),
        { status: 201 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    await sendCommand("RESET");
    const [, init] = mock.mock.calls[0] as [string, RequestInit];
    expect(JSON.parse(init.body as string)).toEqual({ type: "RESET" });
  });

  it("START_PATROL 命令不带 direction", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          commandId: "cmd-00000001",
          type: "START_PATROL",
          result: "PATROL_STARTED",
          robot: robotJson,
        }),
        { status: 201 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    await sendCommand("START_PATROL");
    const [, init] = mock.mock.calls[0] as [string, RequestInit];
    expect(JSON.parse(init.body as string)).toEqual({ type: "START_PATROL" });
  });

  it("后端错误时抛出 ApiError 并携带错误码", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          error: { code: "MISSING_FIELD", message: "缺少 type", requestId: "r1" },
        }),
        { status: 400 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    const error = await fetchRobot().catch((e: unknown) => e);
    expect(error).toBeInstanceOf(ApiError);
    expect(error).toMatchObject({ code: "MISSING_FIELD" });
  });

  it("fetchPerceptionLatest 请求正确路径", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          enabled: true,
          instanceId: "i1",
          observation: null,
          activeDetections: [],
          latestEventId: 0,
        }),
        { status: 200 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    const latest = await fetchPerceptionLatest();
    expect(latest.enabled).toBe(true);
    expect(mock).toHaveBeenCalledWith(
      "/api/v1/perception/latest",
      expect.objectContaining({ signal: expect.anything() }),
    );
  });

  it("fetchPerceptionEvents 携带 after 与 limit 查询参数", async () => {
    const mock = vi.fn().mockResolvedValue(
      new Response(
        JSON.stringify({
          instanceId: "i1",
          events: [],
          oldestEventId: null,
          latestEventId: 0,
          hasMore: false,
          truncated: false,
        }),
        { status: 200 },
      ),
    );
    vi.stubGlobal("fetch", mock);

    const view = await fetchPerceptionEvents(3, 50);
    expect(view.latestEventId).toBe(0);
    expect(mock).toHaveBeenCalledWith(
      "/api/v1/perception/events?after=3&limit=50",
      expect.objectContaining({ signal: expect.anything() }),
    );
  });
});
