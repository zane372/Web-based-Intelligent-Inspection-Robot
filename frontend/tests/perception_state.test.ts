import { describe, expect, it } from "vitest";
import {
  createInitialPerceptionState,
  reducePerception,
  MAX_POLL_FAILURES,
  MAX_EVENTS,
} from "../src/perception_state";
import type {
  Detection,
  PerceptionEvent,
  PerceptionLatest,
  RangeReading,
} from "../src/types";

function detection(id: string, overrides: Partial<Detection> = {}): Detection {
  return {
    hazardId: id,
    hazardType: "SMOKE",
    severity: "HIGH",
    label: null,
    position: { x: 3, y: 3 },
    distanceCells: 1,
    ...overrides,
  };
}

function reading(
  sensorId: RangeReading["sensorId"],
): RangeReading {
  return {
    sensorId,
    relativeDirection:
      sensorId === "range-front"
        ? "FRONT"
        : sensorId === "range-left"
          ? "LEFT"
          : "RIGHT",
    absoluteDirection: "RIGHT",
    hitType: "NONE",
    distanceCells: null,
    cell: null,
  };
}

function latest(overrides: Partial<PerceptionLatest> = {}): PerceptionLatest {
  return {
    enabled: true,
    instanceId: "i1",
    observation: {
      sequence: 0,
      rangeCells: 5,
      ranges: [
        reading("range-front"),
        reading("range-left"),
        reading("range-right"),
      ],
      detections: [],
    },
    activeDetections: [],
    latestEventId: 0,
    ...overrides,
  };
}

function event(id: number, overrides: Partial<PerceptionEvent> = {}): PerceptionEvent {
  return {
    eventId: id,
    instanceId: "i1",
    sequence: id,
    type: "ANOMALY_DETECTED",
    position: { x: 3, y: 3 },
    hazard: detection("h1"),
    contactResult: null,
    ...overrides,
  };
}

describe("perception reducer", () => {
  it("初始为 LOADING", () => {
    expect(createInitialPerceptionState().ui).toBe("LOADING");
  });

  it("disabled 进入 DISABLED", () => {
    const s = reducePerception(createInitialPerceptionState(), { type: "disabled" });
    expect(s.ui).toBe("DISABLED");
    expect(s.observation).toBeNull();
  });

  it("ready 进入 READY 并记录已发现异常", () => {
    const d = detection("h1");
    const s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest({ activeDetections: [d] }),
    });
    expect(s.ui).toBe("READY");
    expect(s.activeDetections).toHaveLength(1);
    expect(s.discovered.map((x) => x.hazardId)).toEqual(["h1"]);
  });

  it("pollOk 累积已发现异常（去重 hazardId）", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest({ activeDetections: [detection("h1")] }),
    });
    s = reducePerception(s, {
      type: "pollOk",
      data: latest({ activeDetections: [detection("h2"), detection("h1")] }),
    });
    expect(s.discovered.map((x) => x.hazardId).sort()).toEqual(["h1", "h2"]);
  });

  it("instanceId 变化清空事件游标与已发现异常", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest({ activeDetections: [detection("h1")], latestEventId: 5 }),
    });
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: [event(1)], latestEventId: 5, truncated: false },
    });
    expect(s.after).toBe(5);
    s = reducePerception(s, {
      type: "pollOk",
      data: latest({ instanceId: "i2", latestEventId: 0 }),
    });
    expect(s.instanceId).toBe("i2");
    expect(s.events).toHaveLength(0);
    expect(s.after).toBe(0);
    expect(s.discovered).toHaveLength(0);
  });

  it("eventsLoaded 按 eventId 去重并推进游标", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest(),
    });
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: [event(1), event(2)], latestEventId: 2, truncated: false },
    });
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: [event(2), event(3)], latestEventId: 3, truncated: false },
    });
    expect(s.events.map((e) => e.eventId)).toEqual([1, 2, 3]);
    expect(s.after).toBe(3);
  });

  it("eventsLoaded 截断时清空并游标归零", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest(),
    });
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: [event(1)], latestEventId: 10, truncated: false },
    });
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: [], latestEventId: 20, truncated: true },
    });
    expect(s.events).toHaveLength(0);
    expect(s.after).toBe(0);
    expect(s.truncated).toBe(true);
  });

  it("事件列表最多保留最近 MAX_EVENTS 条", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest(),
    });
    const many = Array.from({ length: MAX_EVENTS + 10 }, (_, i) => event(i + 1));
    s = reducePerception(s, {
      type: "eventsLoaded",
      data: { events: many, latestEventId: MAX_EVENTS + 10, truncated: false },
    });
    expect(s.events).toHaveLength(MAX_EVENTS);
    expect(s.events[0].eventId).toBe(11);
    expect(s.events[MAX_EVENTS - 1].eventId).toBe(MAX_EVENTS + 10);
  });

  it("连续 3 次 pollFail 进入 DISCONNECTED", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest(),
    });
    s = reducePerception(s, { type: "pollFail" });
    s = reducePerception(s, { type: "pollFail" });
    expect(s.ui).toBe("DEGRADED");
    s = reducePerception(s, { type: "pollFail" });
    expect(s.ui).toBe("DISCONNECTED");
    expect(s.pollFailures).toBe(MAX_POLL_FAILURES);
  });

  it("DISCONNECTED 后 pollOk 恢复 READY", () => {
    let s = reducePerception(createInitialPerceptionState(), {
      type: "ready",
      data: latest(),
    });
    s = reducePerception(s, { type: "disconnect" });
    expect(s.ui).toBe("DISCONNECTED");
    s = reducePerception(s, { type: "pollOk", data: latest() });
    expect(s.ui).toBe("READY");
    expect(s.pollFailures).toBe(0);
  });
});
