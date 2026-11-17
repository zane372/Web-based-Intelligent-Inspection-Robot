---
name: 智能巡检机器人前端
description: 二维网格地图仿真控制台的「定向越野地图」视觉系统
colors:
  course: "#7c3aed"
  course-strong: "#5b21b6"
  signal: "#1a7f3d"
  danger: "#b91c1c"
  caution: "#b45309"
  caution-deep: "#ea580c"
  neutral-bg: "#e7e2d1"
  neutral-surface: "#fbf9f2"
  neutral-surface-soft: "#f1ede0"
  neutral-line: "#d6cfb8"
  neutral-line-strong: "#b3a98d"
  neutral-ink: "#23301f"
  neutral-ink-soft: "#4a5644"
  neutral-ink-muted: "#55614b"
  neutral-ink-faint: "#9aa390"
  field: "#f6f8f0"
  grid: "#cfc6ac"
  obstacle: "#3c5a3f"
  boundary: "#3a2e1f"
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
    fontSize: "12px"
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
  xl: "40px"
components:
  button-primary:
    backgroundColor: "{colors.course}"
    textColor: "#ffffff"
    rounded: "{rounded.md}"
    padding: "0 18px"
    size: "48px"
  button-primary-active:
    backgroundColor: "{colors.signal}"
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

**Creative North Star: "定向越野地图"**

这是一块为巡检机器人的二维地图仿真控制台定制的「定向越野地图」界面：整块界面像一张摊开在桌面上的纸质定向越野地图，网格是地形，机器人是地图上被圈出的检查点（控制点），巡检路径是压在一切之上的紫色航线。暖纸色背景上铺着极淡的紫绿光晕，代替纯白，让页面有「纸张」的温感而不是「白板」的冷感。

界面的气质是「像读一张地图」：信息用制图符号语言组织——符号图例、比例尺、指北针、等高线式的网格——而不是用装饰性渐变或同款灰色圆角卡片堆叠。紫色是唯一的强调色，代表航线与控制点，稀缺而醒目；绿是密林障碍，红是危险，琥珀是边界警示。

**Key Characteristics:**
- 紫色航线/控制点是全场唯一的强调色，压在所有地形之上
- 地图画布是「地形」：每 5 格一条深色计曲线、右上角指北针、符号图例、比例尺
- 机器人画成定向越野控制点（紫环 + 墨色实体 + 紫色朝向三角）
- 已走轨迹渲染为紫色航线（折线压在地形之上、机器人之下）
- 地形有确定性 per-cell 质感染色（约 12% 格略绿/略暖），让网格读作等高线而非加粗方格
- 暖纸色背景 + 淡紫/绿/琥珀径向光晕（非纯白），像摊开的纸质地图

## Colors

暖纸色的中性底上，紫色航线作为唯一强调色，绿/红/琥珀分别表达密林、危险、警示三种语义。颜色全部平涂实色，靠语义区分而不是色相堆叠。

### Primary
- **航线紫 Course** (#7c3aed): 唯一强调色。用于机器人控制点、朝向、激活态、方向键 hover、标题前的 course-mark、已走轨迹的紫色航线。深调 #5b21b6 用于 hover 加深。

### Secondary
- **信号绿 Signal Green** (#1a7f3d): 运行态。用于「自动巡检」激活态、状态正常指示灯，也用作地图里「密林障碍」的底色语义。

### Tertiary
- **危险红 Danger** (#b91c1c): 危险态。用于「重新连接」按钮、HIGH 严重度异常、故障报警闪框。
- **中危琥珀 Caution** (#b45309): LOW 严重度异常、边界警示内线、琥珀指示灯。
- **深橙 Caution Deep** (#ea580c): MEDIUM 严重度异常。

### Neutral
- **暖纸底 Paper** (#e7e2d1): 页面背景，叠加极淡紫/绿径向光晕。
- **表面 Surface** (#fbf9f2): 面板、地图框背景，暖白。
- **软表面 Surface Soft** (#f1ede0): 读出行、次级按钮背景。
- **墨绿 Ink** (#23301f): 主文字，从深林地相调出的墨绿，非纯黑。soft #4a5644 / muted #55614b / faint #9aa390 三级降调（muted 对浅底 ≥4.5:1，faint 仅装饰）。
- **线 Line** (#d6cfb8) / **强线 Line Strong** (#b3a98d): 边框与分隔，从纸色相调出的土色。

### Canvas
- **地形底 Field** (#f6f8f0): 地图画布背景，极浅的苔原绿白。
- **网格细线 Grid** (#cfc6ac): 地图普通网格线（等高线的浅计曲线）。
- **密林障碍 Obstacle** (#3c5a3f): 障碍物填充，深森林绿。
- **地图边界 Boundary** (#3a2e1f): 地图外框与北向箭头，深棕。
- **地形质感 Terrain A/B** (#eaf0e0 / #f0eeda): 确定性 per-cell 染色的略绿/略暖两色，给地形「植被/裸露地表」的质感变化，让网格读作等高线而非加粗方格。

> **颜色单一来源（新增）**：canvas / JSX 侧所有色值收敛到 `frontend/src/theme.ts`（`palette` 与 `severityHex`），`index.css` 的 `@theme` 块是其 CSS 镜像。修改颜色时两处同步。

### Named Rules
**The Course-Over-Everything Rule.** 紫色是唯一的「强调」色，代表巡检航线，压在所有地形之上；机器人控制点、朝向、激活态、hover 都用它。紫的稀缺正是它的意义。
**The Terrain-Semantic Rule.** 绿=密林障碍，红=危险/故障/异常，琥珀=警示/边界，紫=航线/控制点。同一语义永不换色。

## Typography

**Display Font:** Segoe UI（回退 PingFang SC / Microsoft YaHei / Helvetica Neue / Arial / system-ui）
**Body Font:** Segoe UI（同上）
**Label/Mono Font:** ui-monospace / Cascadia Mono / Consolas（用于读数与序号）

**Character:** 系统无衬线为主，等宽字体只出现在「读数」这一种场合（数字对齐），避免把等宽当「技术感」装饰。

### Hierarchy
- **Display** (700, 30px, 1.2, -0.01em): 页头主标题「智能巡检机器人」，唯一大字。
- **Body** (400, 14px, 1.6): 正文、帮助文案。
- **Label** (700, 12px, +0.12em, 大写): 「地图图例」式分区小标题（运行读数/方向控制/符号图例/三向测距等）。
- **Mono** (600, 14px, tabular-nums): 所有数值读数（坐标、序列号、测距、事件 ID）。

### Named Rules
**The Instrument-Readout Rule.** 凡是数字，必用等宽 + tabular-nums，让它像地图坐标读数一样对齐稳定；文字用系统无衬线，绝不混用。

## Layout

两栏网格 `minmax(0,1fr) 380px`，容器 max-width 1760px，左右留白 40px，上下 32px。左栏地图区（弹性占据剩余空间，地图 + 图例垂直居中），右栏控制区与感知面板堆叠（380px 固定）。响应式：≤1100px 时两栏堆叠为单列。间距节奏 8/12/16/24/40px。

## Elevation & Depth

**The Flat-By-Default Rule.** 界面以暖纸底 + 1px 边框的「平面分层」为主，阴影只用于地图框（0 6px 24px 抬升）与主按钮（0 3px 10px 紫色光晕）这两个需要「浮起」的焦点，作为响应式反馈而非装饰。信号灯用柔边光晕（box-shadow 扩散）表达发光。

## Shapes

**The Soft-Square Rule.** 直角被压成 12–14px 的轻微圆角（面板/地图框 14px，按钮/方向键 12px，读出行 10px），整体仍是方正、像地图卡片纸的几何；只有指示灯与状态胶囊是正圆（999px）。地图画布保持 1:1 正方形（aspect-ratio 1/1），这是「地图」的核心几何。

## Components

### Buttons
- **Shape:** 12px 圆角，min-height 48px（触控目标充足）。
- **Primary (patrol-btn):** 紫色底白字 + 紫色柔光晕；激活态（停止巡检）切换为信号绿底白字。
- **Secondary (action-btn):** 软表面底 + 强线边框，hover 加深为 line 色。
- **Direction (dir-btn):** 52×52px 正方形，表面底 + 强线边框，hover 填紫底白字，active 下压 1px。
- **Danger (reconnect-btn):** 危险红底白字，仅断线时出现。

### Cards / Containers
- **Corner Style:** 14px（panel / map-frame）。
- **Background:** 暖白表面。
- **Shadow Strategy:** 面板 0 2px 10px 极轻；地图框 0 6px 24px 抬升（见 Elevation）。
- **Border:** 1px 土色线。

### Status Pill / Signal Lamp
- **Status Pill:** 暖白底 + 土色强线边框 + 正圆，内含信号灯 + 状态文字，`role=status` + `aria-live=polite`。
- **Signal Lamp:** 12px 正圆，绿/琥珀/红/灰四态，柔边光晕；巡逻报警时脉冲动画（1s exponential ease-out）。

### Course Mark（签名组件）
- **Style:** 标题前的紫色竖条（8px 宽、28px 高），像地图上被圈出的航线标记，全场唯一的「强调」视觉签名。

### Course Trail（签名组件 · 新增）
- **Style:** 已走轨迹的紫色折线，连接机器人走过的单元格中心，压在地形之上、机器人之下，像定向越野地图上压在一切之上的课程线。随 instanceId 变化（重置/重载）清零。

### Map Legend（签名组件）
- **Style:** 地图下方的制图图例，含符号图例（密林障碍/机器人控制点/已发现异常/地图边界四符号）、比例尺（10 格色条）、指北箭头与地图信息（尺寸·障碍数·原点），与地图同宽居中。

### Readout Row（签名组件）
- **Style:** 软表面底 + 1px 土色线边框 + 10px 圆角，左标签右数值；数值用等宽 tabular-nums，像地图坐标读数。

### Compass（签名组件）
- **Style:** 方向键中央的朝向罗盘 SVG，指针随 heading 旋转（transition 0.2s），紫底白字，像地图上的指北针。

## Do's and Don'ts

### Do:
- **Do** 用等宽 + tabular-nums 渲染所有数值读数。
- **Do** 用紫=航线/控制点、绿=密林、红=危险的语义色，且同一语义始终用同一色。
- **Do** 给每个交互元素 44px 以上的点击区域和清晰的 `:focus-visible` 紫色环。
- **Do** 用控制点、指北针、计曲线、比例尺这些制图符号贯穿页面，而不是引入新的装饰图案。
- **Do** 保留地图的「红色扩散粒子效果」（blocked 时红环扩散动画）作为越界反馈。

### Don't:
- **Don't** 使用渐变文字、玻璃模糊、彩色侧边条或 >1px 的彩色边框装饰。
- **Don't** 用 emoji 或 Unicode 符号当图标，统一用描边一致的 SVG。
- **Don't** 把等宽字体当作「技术感」用在代码/数据/测量之外的文字上。
- **Don't** 引入纯灰阶或纯黑白的第三套色，所有中性色都从暖纸/墨绿色相调出。
- **Don't** 用硬偏移阴影（4px 4px 0）或 sparkline/进度环/软圆角矩形当作内容容器。
- **Don't** 把页面背景设成单一纯白，背景应保持暖纸色 + 淡紫/绿光晕的「纸张」温感。
