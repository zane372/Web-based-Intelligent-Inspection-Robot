---
name: 智能巡检机器人前端
description: 二维网格地图仿真控制台的「工业安全现场」视觉系统
colors:
  primary: "#b45309"
  primary-deep: "#92400e"
  secondary: "#1a7f3d"
  secondary-deep: "#11612e"
  danger: "#bd2a22"
  caution: "#d97706"
  caution-deep: "#ea580c"
  hazard: "#e2a50c"
  neutral-bg: "#eceff3"
  neutral-surface: "#ffffff"
  neutral-surface-soft: "#f5f7f9"
  neutral-line: "#dbe1e8"
  neutral-line-strong: "#c3ccd6"
  neutral-ink: "#1b2531"
  neutral-ink-soft: "#455167"
  neutral-ink-muted: "#5b6b82"
  neutral-ink-faint: "#a7b0bd"
  field: "#f2f5f7"
  grid: "#d9e0e7"
  obstacle: "#26303c"
typography:
  display:
    fontFamily: "Segoe UI, PingFang SC, Microsoft YaHei, Helvetica Neue, Arial, system-ui, sans-serif"
    fontSize: "30px"
    fontWeight: 700
    lineHeight: 1.2
    letterSpacing: "-0.01em"
  body:
    fontFamily: "Segoe UI, PingFang SC, Microsoft YaHei, Helvetica Neue, Arial, system-ui, sans-serif"
    fontSize: "14px"
    fontWeight: 400
    lineHeight: 1.6
  label:
    fontFamily: "Segoe UI, PingFang SC, Microsoft YaHei, Helvetica Neue, Arial, system-ui, sans-serif"
    fontSize: "11px"
    fontWeight: 700
    letterSpacing: "0.12em"
  mono:
    fontFamily: "ui-monospace, SFMono-Regular, Cascadia Mono, Consolas, Courier New, monospace"
    fontSize: "14px"
    fontWeight: 600
rounded:
  sm: "10px"
  md: "12px"
  lg: "14px"
  full: "999px"
spacing:
  xs: "8px"
  sm: "12px"
  md: "16px"
  lg: "24px"
  xl: "44px"
components:
  button-primary:
    backgroundColor: "{colors.secondary}"
    textColor: "#ffffff"
    rounded: "{rounded.md}"
    padding: "0 18px"
    size: "48px"
  button-primary-active:
    backgroundColor: "{colors.primary}"
    textColor: "#ffffff"
    rounded: "{rounded.md}"
    padding: "0 18px"
    size: "48px"
  button-secondary:
    backgroundColor: "{colors.neutral-surface-soft}"
    textColor: "{colors.neutral-ink}"
    rounded: "{rounded.md}"
    padding: "0 18px"
    size: "48px"
  status-pill:
    backgroundColor: "{colors.neutral-surface}"
    textColor: "{colors.neutral-ink-soft}"
    rounded: "{rounded.full}"
    padding: "7px 14px"
---

# Design System: 智能巡检机器人前端

## Overview

**Creative North Star: "工业安全现场"**

这是一块为巡检机器人的二维地图仿真控制台定制的「工业安全现场」界面：明亮的浅灰现场底上，用安全琥珀、信号绿、信号红三种语义色做状态与动作，用警戒带、信号灯、仪器读数这些真实工业现场的物件语言组织界面，而不是用装饰性渐变或柔和圆角「美化」它。

界面的气质是「明确、实心、像现场设备」：每一个可交互元素都有清晰的几何轮廓、可见的状态变化和明确的语义色。信息以「读出行」的仪器面板形式成组呈现，数字一律等宽、可对齐，像工业仪表读数一样稳定。密度中等偏紧，适合桌面监控与投影演示。

**Key Characteristics:**
- 三种安全信号色各司其职：琥珀（主 accent/警示）、绿（运行/开始）、红（危险/故障）
- 警戒带（黄黑斜纹）与信号灯是贯穿全场的视觉签名
- 等宽数字读数（仪器面板质感），信息按「读出行」成组
- 明亮浅底 + 深墨色文字，对比清晰，适合明亮环境

## Colors

冷灰蓝的中性底上，点缀安全琥珀、信号绿、信号红三种语义色。颜色不做渐变，全部是平涂的实色，靠语义区分而不是靠色相堆叠。

### Primary
- **安全琥珀 Safety Amber** (#b45309): 主 accent。用于自动巡检激活态、方向键 hover、中危警告。深调 #92400e 用于 hover 加深。

### Secondary
- **信号绿 Signal Green** (#1a7f3d): 运行态。用于「自动巡检」主按钮、状态正常指示灯。深调 #11612e 用于 hover。

### Tertiary
- **信号红 Signal Red** (#bd2a22): 危险态。用于「重新连接」按钮、HIGH 严重度异常、故障报警闪框。
- **中危橙 Caution** (#d97706): LOW 严重度异常、边界警示内线、琥珀指示灯。
- **深橙 Caution Deep** (#ea580c): MEDIUM 严重度异常（介于 caution 与 danger 之间）。

### Neutral
- **现场纸底 Paper** (#eceff3): 页面背景。
- **表面 Surface** (#ffffff): 面板、地图框、按钮背景。
- **软表面 Surface Soft** (#f5f7f9): 读出行、次级按钮背景。
- **墨色 Ink** (#1b2531): 主文字。从现场工装的黑蓝色相调出，非纯黑。soft #455167 / muted #5b6b82 / faint #a7b0bd 三级降调（muted 对浅底 ≥4.5:1，faint 仅装饰）。
- **线 Line** (#dbe1e8) / **强线 Line Strong** (#c3ccd6): 边框与分隔。

### Canvas
- **现场浅底 Field** (#f2f5f7): 地图画布背景。
- **网格细线 Grid** (#d9e0e7): 地图网格线。
- **障碍深炭 Obstacle** (#26303c): 障碍物填充。

### Named Rules
**The One-Accent Rule.** 安全琥珀是唯一的「强调」色，仅出现在激活/警示/悬停这些需要引起注意的瞬间；大面积填充只允许信号绿（开始）与信号红（危险）这两个语义明确的动作色。琥珀的稀缺正是它的意义。
**The Semantic-Color Rule.** 绿=运行/开始，红=危险/故障，琥珀=警示/激活。同一语义永不换色。

## Typography

**Display Font:** Segoe UI（回退 PingFang SC / Microsoft YaHei / Helvetica Neue / Arial / system-ui）
**Body Font:** Segoe UI（同上）
**Label/Mono Font:** ui-monospace / Cascadia Mono / Consolas（用于仪器读数）

**Character:** 系统无衬线为主，等宽字体只出现在「读数」这一种场合（数字对齐），避免把等宽当「技术感」装饰。

### Hierarchy
- **Display** (700, 30px, 1.2, -0.01em): 页头主标题「智能巡检机器人」，唯一大字。
- **Body** (400, 14px, 1.6): 正文、帮助文案。
- **Label** (700, 11px, +0.12em, 大写): 「现场标牌」式小标签（地图/障碍/机器人/方向控制/活动异常等分区标题）。
- **Mono** (600, 14px, tabular-nums): 所有数值读数（坐标、序列号、测距、事件 ID）。

### Named Rules
**The Instrument-Readout Rule.** 凡是数字，必用等宽 + tabular-nums，让它像仪表读数一样对齐稳定；文字用系统无衬线，绝不混用。

## Layout

三栏网格 `minmax(0,1fr) 340px 400px`，容器 max-width 1840px，左右留白 44px，上下 36px。左栏地图区（弹性占据剩余空间）、中栏控制区（340px 固定）、右栏感知面板（400px 固定）。响应式：≤1200px 时地图区跨满两栏、感知面板下移；≤800px 时三栏堆叠为单列。间距节奏 8/12/16/24/44px。

## Elevation & Depth

**The Flat-By-Default Rule.** 界面以浅底 + 1px 边框的「平面分层」为主，阴影只用于地图框（0 6px 24px 抬升）与主按钮（0 3px 10px 色光晕）这两个需要「浮起」的焦点，作为响应式反馈而非装饰。信号灯用柔边光晕（box-shadow 扩散）表达发光。

## Shapes

**The Soft-Square Rule.** 直角被压成 12–14px 的轻微圆角（面板/地图框 14px，按钮/方向键 12px，读出行 10px），整体仍是方正、像设备外壳的几何；只有指示灯与状态胶囊是正圆（999px）。警戒带是唯一带「斜纹图案」的元素，其余一律平涂。

## Components

### Buttons
- **Shape:** 12px 圆角，min-height 48px（触控目标充足）。
- **Primary (patrol-btn):** 信号绿底白字 + 绿色柔光晕；激活态（停止巡检）切换为安全琥珀底白字。
- **Secondary (action-btn):** 软表面底 + 强线边框，hover 加深为 line 色。
- **Direction (dir-btn):** 52×52px 正方形，表面底 + 强线边框，hover 填琥珀底白字，active 下压 1px。
- **Danger (reconnect-btn):** 信号红底白字，仅断线时出现。

### Cards / Containers
- **Corner Style:** 14px（panel / map-frame）。
- **Background:** 表面白。
- **Shadow Strategy:** 面板 0 2px 10px 极轻；地图框 0 6px 24px 抬升（见 Elevation）。
- **Border:** 1px 线色。

### Status Pill / Signal Lamp
- **Status Pill:** 白底 + 强线边框 + 正圆，内含信号灯 + 状态文字，`role=status` + `aria-live=polite`。
- **Signal Lamp:** 12px 正圆，绿/琥珀/红/灰四态，柔边光晕；巡逻报警时脉冲动画（1s exponential ease-out）。

### Hazard Tape（签名组件）
- **Style:** 黄黑斜纹（-45° repeating-linear-gradient，黑 14px / 警戒黄 14px），置于页头顶部作为视觉签名，全场唯一带图案的元素。

### Readout Row（签名组件）
- **Style:** 软表面底 + 1px 线边框 + 10px 圆角，左标签右数值；数值用等宽 tabular-nums。

## Do's and Don'ts

### Do:
- **Do** 用等宽 + tabular-nums 渲染所有数值读数。
- **Do** 用语义色（绿/红/琥珀）表达状态，且同一语义始终用同一色。
- **Do** 给每个交互元素 44px 以上的点击区域和清晰的 `:focus-visible` 琥珀环。
- **Do** 用警戒带 + 信号灯这两处签名贯穿页面，而不是引入新的装饰图案。

### Don't:
- **Don't** 使用渐变文字、玻璃模糊、彩色侧边条或 >1px 的彩色边框装饰。
- **Don't** 用 emoji 或 Unicode 符号当图标，统一用描边一致的 SVG。
- **Don't** 把等宽字体当作「技术感」用在代码/数据/测量之外的文字上。
- **Don't** 引入纯灰阶或纯黑白的第三套色，所有中性色都从现场工装的墨蓝色相调出。
- **Don't** 用硬偏移阴影（4px 4px 0）或 sparkline/进度环/软圆角矩形当作内容容器。
