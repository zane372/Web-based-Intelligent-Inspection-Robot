import { Component } from "react";
import type { ErrorInfo, ReactNode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import "./index.css";

interface BoundaryState {
  error: Error | null;
}

// 渲染层意外异常时给出降级界面与恢复入口，避免整树卸载白屏。
class ErrorBoundary extends Component<{ children: ReactNode }, BoundaryState> {
  state: BoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): BoundaryState {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo): void {
    console.error("前端渲染异常", error, info);
  }

  render(): ReactNode {
    if (this.state.error) {
      return (
        <div className="panel mx-auto mt-16 max-w-md p-8 text-center text-sm text-ink-soft">
          <h1 className="m-0 mb-3 text-lg font-bold text-ink">界面加载失败</h1>
          <p className="mb-5">发生了意外错误，请尝试重新加载。</p>
          <button
            type="button"
            className="action-btn"
            onClick={() => window.location.reload()}
          >
            重新加载
          </button>
        </div>
      );
    }
    return this.props.children;
  }
}

const root = document.getElementById("root");
if (!root) {
  throw new Error("缺少 #root 挂载点，请检查 index.html");
}
createRoot(root).render(
  <ErrorBoundary>
    <App />
  </ErrorBoundary>,
);
