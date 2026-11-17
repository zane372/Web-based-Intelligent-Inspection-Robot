import { useEffect, useReducer } from "react";
import { usePerceptionController } from "./hooks/usePerceptionController";
import { useRobotController } from "./hooks/useRobotController";
import {
  createInitialPerceptionState,
  reducePerception,
} from "./perception_state";
import { createInitialState, reduce } from "./state";
import type { Heading } from "./types";
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

  // 键盘加速器：方向键/WASD 移动，空格切换巡检（输入框与按钮聚焦时让位给原生行为）。
  useEffect(() => {
    const onKeyDown = (e: KeyboardEvent): void => {
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
    <main className="mx-auto flex w-full max-w-[1840px] flex-col gap-7 px-11 py-9 max-[800px]:gap-5 max-[800px]:px-4 max-[800px]:py-5">
      <StatusBar state={state} />
      <div className="grid grid-cols-[minmax(0,1fr)_340px_400px] items-start gap-10 max-[1200px]:grid-cols-[minmax(0,1fr)_minmax(340px,400px)] max-[800px]:grid-cols-1 max-[800px]:gap-6">
        <MapPanel
          className="max-[1200px]:col-span-2 max-[800px]:col-span-1"
          map={state.map}
          robot={state.robot}
          ui={state.ui}
          discovered={perception.discovered}
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
