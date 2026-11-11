# Web 智能巡检机器人二维仿真系统

一套纯软件实现的二维机器人仿真系统。用户在浏览器中查看二维网格地图（含障碍物），通过「上、下、左、右」按钮逐格手动控制机器人，或一键开启自动巡检；后端校验命令并驱动虚拟机器人改变状态，再把位置、朝向、运行状态、巡检状态与命令结果返回前端。

> 本仓库在 SPEC.md **基础版**之上，扩展了标准版的多项能力：**地图障碍物**（阻挡移动）、**自动巡检**（Boustrophedon 单元分解 + 贪心 BFS 组合策略覆盖遍历）与**虚拟感知**（三向测距、异常检测与事件、独立感知面板）。不包含真实传感器硬件、AI Agent 等更高阶能力。

## 功能特性

- 二维网格地图（默认 `30 × 30`，含 40 个障碍物，`config/map.json` 可配置），Canvas 绘制网格、边界、障碍物、机器人位置与朝向。
- 桌面两栏布局：左侧放大网格地图，右侧竖排读数面板、方向键、重置、自动巡检按钮与使用帮助；手动控制每次移动一格。
- 边界保护：越界尝试不改变位置，但更新朝向并反馈 `BLOCKED_BOUNDARY`。
- 障碍物阻挡：移动目标为障碍物时位置不变、朝向更新并反馈 `BLOCKED_OBSTACLE`。
- 自动巡检：一键遍历全部可通行格，覆盖路径由 **Boustrophedon 单元分解**（矩形单元蛇形扫描）与 **贪心 BFS** 两种规划器竞争取更短者；可随时停止，巡检期间位置与 `sequence` 实时刷新。巡检过程带障碍物/边界检测：若异常命中障碍物或边界，机器人报警（`BLOCKED_OBSTACLE`／`BLOCKED_BOUNDARY`）并自动重规划绕行路径，前端锁存显示「巡检碰到障碍物，正在绕行」或「巡检到达边界，正在绕行」。
- 秒级状态轮询、页面隐藏暂停、断线重连、实例变化自动重载。
- 虚拟感知：三向测距（前/左/右，命中障碍物或边界时报告距离与命中格）、异常检测（烟雾/高温/气体泄漏/油污泄漏按曼哈顿距离判定，每次重置后异常源位置随机）、事件流（异常进入/离开、边界/障碍接触），独立感知面板展示三向命中、活动异常与最近事件；感知故障不干扰手动控制与巡检。
- 严格 RESTful API 契约、统一错误结构、命令序列号 `sequence` 单调递增。
- C++ 单元测试、TypeScript 前端测试、Python 黑盒集成测试三层质量保障。
- 多阶段 Docker 镜像 + Docker Compose 一键启动 + GitHub Actions 基础 CI + CD 工作流（GHCR digest、Staging、生产审批与回滚）。

## 技术栈

| 层次 | 技术 |
| --- | --- |
| 后端 | C++20（GCC 12+）、Crow（HTTP）、nlohmann/json 3.11.x |
| 仿真核心 | 纯 C++（`domain/`、`control/`、`perception/`，不依赖 Crow／JSON／Canvas） |
| 构建 | CMake 3.24+、Ninja、GoogleTest + CTest |
| 前端 | TypeScript 5.x、Vite、原生 Canvas 2D、Vitest、ESLint |
| 集成测试 | Python 3.12、pytest 8.x、requests 2.32.x |
| 交付 | Docker Compose v2、GitHub Actions |

## 目录结构

```
├── backend/                # C++ 后端
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp        # 入口：配置加载、依赖组装、启动 HTTP
│   │   ├── domain/         # 仿真领域模型（位置、朝向、网格、障碍物、机器人、巡检规划）
│   │   ├── control/        # RobotController（序列号、互斥、快照）+ PatrolExecutor（巡检线程）
│   │   ├── perception/     # 虚拟感知（测距、异常检测、事件环形缓冲、感知服务）
│   │   ├── api/            # HTTP 路由、命令校验、DTO 映射
│   │   ├── config.cpp      # config/map.json 与 config/perception.json 加载与校验
│   │   └── dto.hpp         # 枚举 <-> 字符串、JSON DTO
│   └── tests/              # GoogleTest 单元测试
├── frontend/               # TypeScript 前端
│   ├── src/{main,state,types}.ts
│   ├── src/api/client.ts   # 统一 API 客户端
│   ├── src/canvas/renderer.ts
│   └── tests/              # Vitest 测试
├── config/map.json         # 地图与初始姿态配置
├── config/perception.json  # 虚拟感知配置（测距范围、异常半径、异常源初始位置，重置后随机）
├── tests/integration/      # Python 黑盒集成测试
├── docs/                   # 项目文档与展示材料
│   ├── README.md           # 文档索引
│   ├── request.md          # 课程任务说明
│   ├── agent.md            # 协作与实现规范
│   ├── demo/演示脚本.md    # 结业演示脚本
│   └── images/             # 规格编制依据参考图
├── .github/workflows/ci.yml
├── .github/workflows/cd.yml
├── deploy/compose.yaml      # 按镜像摘要部署的 Compose 配置（APP_IMAGE=image@digest）
├── Dockerfile
├── docker-compose.yml
├── README.md               # 项目说明（本文件）
└── SPEC.md                 # 软件规格说明书（契约基线）
```

## 架构

```
┌─────────────────────────────── 浏览器 ───────────────────────────────┐
│  Canvas 渲染器 · 状态机 · 命令按钮 · 1s 轮询 · 断线重连 · 感知面板   │
└───────────────────────────────┬─────────────────────────────────────┘
                                │ HTTP /api/v1（同源，Crow 静态托管 dist）
┌───────────────────────────────▼─────────────────────────────────────┐
│  API 层（api/）    路由 · JSON 校验 · 统一错误 · 405/413/415        │
├─────────────────────────────────────────────────────────────────────┤
│  Control 层（control/）  RobotController：互斥 · sequence · 快照    │
│                          PatrolExecutor：巡检线程 · 条件变量 · 游标 │
├─────────────────────────────────────────────────────────────────────┤
│  Perception 层（perception/）  三向测距 · 异常检测 · 事件环形缓冲   │
├─────────────────────────────────────────────────────────────────────┤
│  Domain 层（domain/）   Grid 障碍 · Robot 移动/边界/障碍 · 巡检规划 │
└─────────────────────────────────────────────────────────────────────┘
        内存状态（无数据库），服务重启后恢复 config/map.json 初始位姿
```

依赖方向为 `API → Control/Perception → Domain`，Domain 与 Perception 不依赖 Crow／JSON／Canvas。

## 快速开始

### Docker 主路径（推荐）

需要 Docker Compose v2。

```bash
docker compose up --build -d
docker compose ps
curl -fsS http://127.0.0.1:8080/api/v1/health
curl -fsS http://127.0.0.1:8080/api/v1/robot
docker compose logs --no-color app
docker compose down
```

浏览器打开 <http://127.0.0.1:8080>，完成一次移动、一次边界/障碍尝试、一次自动巡检和一次重置。

> 若宿主机 `8080` 端口被占用，可改用其他端口：
> - Linux／macOS：`HOST_PORT=8081 docker compose up --build -d`
> - Windows PowerShell：先 `$env:HOST_PORT = "8081"`，再 `docker compose up --build -d`
>
> 验收地址同步改为 `http://127.0.0.1:8081`。

> **安全边界（对外暴露前必读）**：本系统当前无用户鉴权、无应用层 TLS，默认仅监听 `127.0.0.1`（`docker-compose.yml` 端口映射也绑定宿主机回环）。如需对外提供服务，必须显式设 `APP_HOST=0.0.0.0` 并置于带鉴权的反向代理之后，由反代终结 TLS（HTTPS）并限制请求体大小（如 nginx `client_max_body_size 1k`）——应用层的 4096 字节上限是命令语义校验，Crow 在进入处理前已把整个请求体读入内存，无法阻止超大 `Content-Length` 耗尽内存。

### 本地开发路径（辅助）

**C++ 后端**（需要 CMake、GCC 12+、libboost-system-dev、libasio-dev）：

```bash
cmake -S backend -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
# 启动后端（默认监听 127.0.0.1:8080）
./build/patrol_server
```

**前端**（需要 Node 22）：

```bash
cd frontend
npm ci
npm run dev   # Vite 开发服务器，默认 5173，代理 /api 到 127.0.0.1:8080
```

**Python 集成测试**（服务已启动时执行）：

```bash
python -m venv .venv
# 激活虚拟环境后
python -m pip install -r tests/integration/requirements.txt
pytest tests/integration -q --base-url http://127.0.0.1:8080
```

## API 说明

前缀 `/api/v1`。成功响应不包 `data`；错误统一为 `{ "error": { "code", "message", "details?", "requestId" } }`。

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/v1/health` | 健康检查，返回 `{ status, instanceId }` |
| GET | `/api/v1/map` | 地图信息 `{ mapId, width, height, origin, xAxis, yAxis, obstacles }` |
| GET | `/api/v1/robot` | 机器人状态 `{ robotId, instanceId, position, heading, motionStatus, lastResult, sequence, patrolActive }` |
| POST | `/api/v1/robot/commands` | 命令 `{ type: "MOVE"\|"RESET"\|"START_PATROL"\|"STOP_PATROL", direction? }`，返回 201 |
| GET | `/api/v1/perception/latest` | 最新感知 `{ enabled, instanceId, observation, activeDetections, latestEventId }` |
| GET | `/api/v1/perception/events` | 感知事件流 `?after=0&limit=50`，返回 `{ instanceId, events, oldestEventId, latestEventId, hasMore, truncated }` |

命令请求必须携带 `Content-Type: application/json`，请求体上限 4096 字节。客户端不得提交坐标／朝向／sequence。

- `START_PATROL` 启动自动巡检（无需 `direction`），响应 `result: "PATROL_STARTED"` 且 `robot.patrolActive` 变 `true`；遍历完成后自动停止（`lastResult: "PATROL_COMPLETED"`，`patrolActive` 回 `false`）。
- `STOP_PATROL` 随时停止巡检，响应 `result: "PATROL_STOPPED"`。
- 手动 `MOVE`／`RESET` 会隐式停止正在进行的巡检。
- 命令结果（`result`／`lastResult`）新增：`BLOCKED_OBSTACLE`、`PATROL_STARTED`、`PATROL_STOPPED`、`PATROL_COMPLETED`。

主要错误码：`400 MALFORMED_JSON / MISSING_FIELD / INVALID_COMMAND_TYPE / INVALID_DIRECTION / UNKNOWN_FIELD / INVALID_QUERY_PARAMETER`、`413 PAYLOAD_TOO_LARGE`、`404 ROUTE_NOT_FOUND`、`405 METHOD_NOT_ALLOWED`、`415 UNSUPPORTED_MEDIA_TYPE`、`500 INTERNAL_ERROR`。非法请求不改变状态，也不占用 `sequence`。

**虚拟感知**（由 `PERCEPTION_CONFIG` 环境变量指向 `config/perception.json` 启用；未设置时感知关闭）：

- 三向测距 `range-front/range-left/range-right` 随机器人朝向旋转，逐格前进，命中障碍物（`hitType: OBSTACLE` + 距离 + 命中格）或边界（`hitType: BOUNDARY` + 距离）即停，未命中为 `NONE`。
- 异常检测按曼哈顿距离 `hazardRangeCells` 判定，支持烟雾/高温/气体泄漏/油污泄漏四类异常；进入半径生成 `ANOMALY_DETECTED`、离开生成 `ANOMALY_OUT_OF_RANGE`（保留最后位置，不宣称「恢复」）。每次 `RESET` 后异常源位置重新随机（真值不对外暴露）。
- 首次撞边界/障碍生成 `CONTACT_BOUNDARY`／`CONTACT_OBSTACLE` 事件；事件流按 `eventId` 单调递增、200 条环形缓冲，支持 `after`/`limit` 分页。

## 测试

| 层 | 命令 | 覆盖 |
| --- | --- | --- |
| C++ 单元测试 | `ctest --test-dir build --output-on-failure` | 四方向移动、四边界、障碍阻挡、重置、序列号、并发、巡检规划覆盖、命令校验、测距、异常检测、感知事件、配置校验 |
| 前端测试 | `npm run lint && npm run typecheck && npm test && npm run build` | 状态机、API 客户端、坐标像素映射、障碍物渲染、巡检按钮、感知面板、感知事件去重 |
| Python 集成测试 | `pytest tests/integration -q --base-url http://127.0.0.1:8080` | 健康、地图/状态契约（含障碍物/巡检态）、移动、边界、障碍、重置、自动巡检、错误处理、感知契约、异常/接触事件 |

## 分工与贡献

| 成员／方向 | 主责模块 | 说明 |
| --- | --- | --- |
| 成员 A／实现算法 | `backend/src/domain/`、`backend/tests/` 仿真单元测试 | 离散移动、边界/障碍规则、四方向增量、重置、Boustrophedon 单元分解 + 贪心 BFS 组合策略巡检规划 |
| 成员 B／全栈 | `frontend/`、页面集成、根 README 与展示材料 | Canvas 绘制（含障碍物/异常图层）、API Client、UI 状态机、巡检按钮、感知面板、联调 |
| 成员 C／运维 | `Dockerfile`、`docker-compose.yml`、`.dockerignore`、`.github/workflows/{ci,cd}.yml` | 多阶段构建、非 root、CI 三 job、CD（GHCR digest、Staging、生产审批/回滚） |
| 成员 D／嵌入式（虚拟设备软件） | `backend/src/api/`、`backend/src/control/`、`backend/src/perception/`、`main.cpp`、配置加载 | API 契约、严格校验、序列号、巡检线程控制（PatrolExecutor）、虚拟感知服务、静态托管 |
| 成员 E／人工智能（质量保障） | `tests/integration/`、验收矩阵、测试报告 | pytest 黑盒测试、健康等待、错误路径覆盖、感知契约 |

## 故障排查

- **端口被占用**：按上文 `HOST_PORT` 方式换端口。
- **构建失败**：`docker compose build` 重新构建；C++ 第三方依赖（Crow/json/GoogleTest）在构建阶段从 GitHub 拉取，需网络可达。
- **健康检查失败**：`docker compose ps` 查看容器状态，`docker compose logs --no-color app` 定位；确认 `config/map.json` 合法。

## 结业提交材料

- 代码仓库：本仓库（含提交历史、CI 工作流）。
- README 与运行说明：本文档。
- 架构图：上文「架构」章节。
- 展示材料：`docs/demo/`。
- 分工：上文「分工与贡献」章节。
