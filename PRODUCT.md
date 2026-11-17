# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Stack

React + TypeScript + Vite + Tailwind CSS + Canvas 2D（用户 2026-11-17 明确指定：迁移 React+TS、引入 Tailwind、连 Canvas 内部视觉一起重做）

## Users

- 项目访客 / 演示者：在浏览器查看并操作二维巡检机器人仿真（手动四方向移动、自动巡检、观察虚拟感知），用于课程演示与结业展示。
- 初级开发团队成员：阅读源码与文档，理解并扩展工程（可理解、可测试、可复现）。

## Product Purpose

一套纯软件实现的二维巡检机器人仿真系统：用户在浏览器查看含障碍物的二维网格地图，用方向按钮逐格手动控制机器人或一键自动巡检；后端（C++20）是状态权威，前端只按 API 结果展示；另提供虚拟感知（三向测距、异常检测、事件流）。成功标准 = 演示可运行、代码可理解、结果可验证，而非模拟复杂物理或训练 AI 模型。

## Positioning

离散、确定性的网格仿真，强调「可理解、可测试、可复现」的工程闭环，与真实机器人、物理引擎、AI Agent 明确区分。核心机制 = 后端权威状态 + 前端按契约展示（前端不复制业务判断）。

## Operating Context

- 运行/演示：浏览器（本地 Vite 开发 5173，或 Docker/Traefik 部署 8080/80）；1 秒轮询、断线重连、instanceId 变化自动重载。
- 场景：课程项目 + GitHub Actions CI/CD；演示常在明亮房间或投影下进行（影响明暗主题取向）。
- 材料：README.md、SPEC.md（契约基线）、docs/demo/演示脚本.md。

## Capabilities and Constraints

- 后端 C++20 权威：移动/巡检/感知规则都在后端，前端只展示与发命令（SPEC 4）。
- 移动规则须可脱离浏览器独立测试；状态机（state.ts / perception_state.ts）是纯 reducer。
- 地图默认 30×30、40 障碍；原点左上，x 向右、y 向下。
- 手动控制每次一格；越界/障碍阻止移动并反馈 BLOCKED_BOUNDARY / BLOCKED_OBSTACLE。
- 自动巡检 = Boustrophedon 单元分解 + 贪心 BFS 竞争取短；可随时停止/重置。
- 虚拟感知独立于手动/巡检（感知故障不得断连控制）；只展示「已发现」异常，不泄露未发现异常位置（SPEC 6.8）。
- API 契约严格（统一错误结构、sequence 单调递增、请求体 4096 字节上限）；前端不得提交坐标/朝向/sequence。
- 本次 UI 重设计不得改动后端 API 契约与外部行为（SPEC 4 回归基线）。

## Brand Commitments

- 名称：Web 智能巡检机器人二维仿真系统（仓库 zane372/Web-based-Intelligent-Inspection-Robot）。
- 无既有视觉品牌/logo/设计系统；视觉身份由本次重设计建立（impeccable new-work）。

## Evidence on Hand

- 现有前端：frontend/（index.html、src/{main,state,perception_state,types}.ts、api/client.ts + validate.ts、canvas/renderer.ts、styles.css）。
- 现有测试：frontend/tests/（Vitest，覆盖状态机、API 客户端、坐标像素映射、障碍物渲染、巡检按钮、感知面板、感知事件去重）。
- 现有产品文档：README.md、SPEC.md、docs/。
- 无真实传感器数据、无品牌资产、无用户调研；视觉方向须由推导 + 用户选定，不得虚构。

## Product Principles

1. 任务优先：操作者应能在注意到「世界」之前就知道每个控件在哪（Operate 模式）。
2. 状态透明：位置、朝向、运行状态、最近结果、序列号始终清晰可读；失败明确提示并给出恢复路径。
3. 前后端职责分明：前端只展示权威，不复制业务判断。
4. 可测试优先：状态机、坐标映射、渲染规则保持纯函数，可脱离浏览器验证。
5. 诚实：未验证/未实现/合成内容如实标注，不把规划写成既成事实。

## Accessibility & Inclusion

- 现有约定：aria-label / role=status / aria-live；障碍物图形+文字（非仅颜色）区分；按钮点击区 ≥44×44；键盘 focus 可见；数字用 tabular-nums。重设计须保留并加强这些约定。
