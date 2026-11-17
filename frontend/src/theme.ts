// 设计令牌 · 单一来源（canvas / JSX 侧）。
// index.css 的 @theme 块是这些值的 CSS 镜像（供 Tailwind 类与 var() 使用）。
// 修改颜色时两处需同步，避免 canvas 与 DOM 色值漂移。
export const palette = {
  // 地形（定向越野 ISOM 风）
  field: "#f6f8f0", // 可通行地形
  grid: "#cfc6ac", // 等高线（细）
  gridIndex: "#b3a986", // 计曲线（每 5 格，略深）
  obstacle: "#3c5a3f", // 密林绿（障碍）
  obstacleHatch: "#2f4a35", // 密林纹理斜线
  boundary: "#3a2e1f", // 深土边界
  terrainA: "#eaf0e0", // 地形质感：略绿（植被感）
  terrainB: "#f0eeda", // 地形质感：略暖（裸露地表感）
  // 机器人 / 航线
  robotBody: "#23301f", // 机器人本体（墨色）
  course: "#7c3aed", // 控制点环 + 朝向 + 航线（签名紫）
  north: "#4a5644", // 指北针
  // 状态信号
  signal: "#1a7f3d", // 绿：就绪/巡检中
  danger: "#b91c1c", // 红：阻断/危险（严重度 HIGH）
  caution: "#b45309", // 琥珀：降级（严重度 LOW）
  cautionDeep: "#ea580c", // 深橙（严重度 MEDIUM）
} as const;

// 严重度 → 色：severityColor() 的唯一数据来源。
export const severityHex = {
  LOW: palette.caution,
  MEDIUM: palette.cautionDeep,
  HIGH: palette.danger,
} as const;
