import { useEffect, useReducer, useRef, useState } from "react";
import { usePerceptionController } from "./hooks/usePerceptionController";
import { useRobotController } from "./hooks/useRobotController";
import {
  createInitialPerceptionState,
  reducePerception,
} from "./perception_state";
import { createInitialState, reduce } from "./state";
import type { Heading, Position } from "./types";
import { ControlPanel } from "./components/ControlPanel";
import { MapPanel } from "./components/MapPanel";
import { PerceptionPanel } from "./components/PerceptionPanel";
import { StatusBar } from "./components/StatusBar";

export default function App() {
  const [state, dispatch] = useReducer(reduce, undefined, createInitialState);
  const [perception, dispatchPerception] = useReducer(
    reducePerception,
    undefined,
    createInitialPerceptionState,
  );
  const { move, reset, togglePatrol, reconnect } = useRobotController(
    state,
    dispatch,
  );
  usePerceptionController(perception, dispatchPerception);

  // 已走轨迹：随机器人位置累积，instanceId 变化（重置/重载）时清零。
  // 供 MapPanel 把「紫色航线」画在地形之上、机器人之下。
  const [visited, setVisited] = useState<Position[]>([]);
  const instanceRef = useRef<string | null>(null);
  useEffect(() => {
    const r = state.robot;
    if (!r) {
      instanceRef.current = null;
      setVisited([]);
      return;
    }
    if (instanceRef.current !== r.instanceId) {
      instanceRef.current = r.instanceId;
      setVisited([r.position]);
      return;
    }
    setVisited((prev) => {
      const last = prev[prev.length - 1];
      if (!last || last.x !== r.position.x || last.y !== r.position.y) {
        return [...prev, r.position];
      }
      return prev;
    });
  }, [state.robot]);

  // 键盘加速器：方向键/WASD 移动，空格切换巡检（输入框与按钮聚焦时让位给原生行为）。
  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent): void => {
      // 修饰键组合（复制/保存等）不拦截。
      if (e.ctrlKey || e.altKey || e.metaKey) {
        return;
      }
      const target = e.target as HTMLElement | null;
      const tag = target?.tagName;
      if (
        tag === "INPUT" ||
        tag === "TEXTAREA" ||
        (target?.isContentEditable ?? false)
      ) {
        return;
      }
      const key = e.key.length === 1 ? e.key.toLowerCase() : e.key;
      const dirMap: Record<string, Heading> = {
        ArrowUp: "UP",
        ArrowDown: "DOWN",
        ArrowLeft: "LEFT",
        ArrowRight: "RIGHT",
        w: "UP",
        a: "LEFT",
        s: "DOWN",
        d: "RIGHT",
      };
      const heading = dirMap[key];
      if (heading) {
        e.preventDefault();
        move(heading);
        return;
      }
      if (e.key === " " && tag !== "BUTTON") {
        e.preventDefault();
        togglePatrol();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [move, togglePatrol]);

  return (
    <main className="mx-auto flex h-screen w-full max-w-[2000px] flex-col gap-4 overflow-hidden px-5 py-4 max-[1100px]:h-auto max-[1100px]:min-h-screen max-[1100px]:overflow-visible">
      <StatusBar state={state} />
      <div className="grid min-h-0 flex-1 grid-rows-1 grid-cols-[minmax(0,1fr)_minmax(290px,350px)_minmax(320px,410px)] items-stretch gap-4 max-[1100px]:grid-rows-none max-[1100px]:grid-cols-1 max-[1100px]:flex-none max-[1100px]:gap-4">
        <MapPanel
          map={state.map}
          robot={state.robot}
          ui={state.ui}
          discovered={perception.discovered}
          visited={visited}
        />
        <ControlPanel
          state={state}
          onMove={move}
          onReset={reset}
          onTogglePatrol={togglePatrol}
          onReconnect={reconnect}
        />
        <PerceptionPanel state={perception} />
      </div>
    </main>
  );
}
