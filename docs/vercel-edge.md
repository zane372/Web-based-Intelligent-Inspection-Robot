# Vercel 边缘函数：学习记录与部署说明

> 学习对象是 Vercel 官方文档（2026-11 抓取的 `.md` 原文），目标是搞清「边缘函数怎么配、部署产物长什么样、上线要几步」。可运行示例在 [`deploy/vercel-edge/`](../deploy/vercel-edge/)，13 条行为断言在本地 V8 隔离环境通过。
>
> **部署状态：未部署。** 本机无 Vercel 登录态（`vercel build` 报 `The specified token is not valid`），因此本文不含任何线上 URL，示例也未经真机验证。

## 1. 先看结论：Edge 还是 Node

官方在 Edge Runtime 文档开头直接写了迁移建议：

> We recommend migrating from edge to Node.js for improved performance and reliability. Both runtimes run on Fluid compute with Active CPU pricing.

同页还写明：**从 Next.js 16.3 起，`runtime = 'edge'` 不再被支持**，路由与页面统一跑 Node.js。

所以边缘运行时今天的定位是：**地理就近、轻量请求改写（鉴权、A/B、重定向、代理剥头）**。需要长耗时、大依赖、文件系统、原生模块时一律用 Node.js 运行时。两者同在 Fluid compute 上，都按 Active CPU 计费，切换成本很低。

## 2. 怎么开启

```js
// api/hello.js
export const config = { runtime: "edge" };

export default async function handler(request) {
  return Response.json({ ok: true, region: process.env.VERCEL_REGION });
}
```

- 非框架项目：`package.json` 必须有 `"type": "module"`，或文件用 `.mjs`，否则默认按 CJS 处理、`export` 直接报错。
- 指定区域：`export const config = { runtime: "edge", regions: ["hnd1"] }`；不写则下发 `regions: "all"`，请求按就近原则调度。
- 线上区域由 `process.env.VERCEL_REGION` 回显；本地/裸 V8 环境没有 `process`，取值要写成 `typeof process !== "undefined" && process.env?.VERCEL_REGION`（这个坑实测踩过）。
- 判别运行时：`globalThis.EdgeRuntime` 是否存在。

## 3. 能用什么、不能用什么

| 可用 | 不可用 |
| --- | --- |
| `fetch` / `Request` / `Response` / `Headers` / `FormData` / `Blob` / `URL` / `URLPattern` | 文件系统（`fs`） |
| `TextEncoder/Decoder`、Web Streams、`crypto`（含 `subtle`）、`AbortController`、`structuredClone` | `require`、`eval`、`new Function`、`WebAssembly.compile`（只能 import `.wasm`） |
| `async_hooks`、`events`、`buffer`、`assert`、`util`（`Buffer` 为全局） | 其他所有 Node 内置模块、任何依赖原生二进制的包 |

实测证据（`@edge-runtime/vm`，与线上同为 V8 isolate 语义）：

- `eval("1+1")` → `EvalError: Code generation from strings disallowed for this context`
- `await import("node:fs")` → `TypeError: A dynamic import callback was not specified.`
- `crypto.subtle.sign` 正常，因此 HMAC 签名校验可以在边缘完成，但要用 `crypto.subtle` 而不是 Node 的 `crypto.createHmac`。

## 4. 限制（决定架构的那几个数）

| 项 | 值 |
| --- | --- |
| 开始响应 | 25 秒内必须发出首个字节 |
| 响应总时长 | 可流式至 300 秒 |
| 代码体积（gzip） | Hobby 1 MB / Pro 2 MB / Enterprise 4 MB |
| 请求体 / 非流式响应体 | 4.5 MB，超限 `413 FUNCTION_PAYLOAD_TOO_LARGE`；流式响应不受限 |
| 单实例内存 | 只能在 Dashboard → Settings → Functions 设（Fluid compute 下 `memory` 不写进 `vercel.json`） |
| 并发 | 自动扩缩，Hobby/Pro 上限 30,000 |

响应超时返回 `504 FUNCTION_INVOCATION_TIMEOUT`。要把大响应转出去，必须 `return new Response(upstream.body, ...)` 流式透传，而不是先 `await upstream.text()`。

## 5. 部署产物：`.vc-config.json` 与 Build Output API

这是「部署」真正落地的一层，很多故障都出在这里。

```
.vercel/output/
├── config.json                    # 路由表：rewrites / redirects / headers / cleanUrls
├── static/                        # 静态文件
└── functions/
    └── api/hello.func/
        ├── .vc-config.json        # 该函数的运行时配置
        └── index.js               # 打包后的入口
```

```jsonc
// .vc-config.json（Edge）
{ "runtime": "edge", "entrypoint": "index.js", "regions": "all" }
```

要点：

- `.func` 后缀**不进 URL**：`api/hello.func` 对外就是 `/api/hello`。
- 未写 `regions` 时等价于 `"all"`（全网分发），不是「默认 iad1」——iad1 是 Node.js 函数的默认单区域，两者不同。
- `.func` 目录里只允许 `.js` / `.json` / `.wasm`。
- `vercel build` 生成产物，`vercel deploy --prebuilt` 只上传产物（源码不进 Vercel）。CI 里做构建、再用 prebuilt 部署，就是这个链路。

## 6. 从源码到线上：七步

1. 触发（Git push / `vercel deploy` / CI 调 API）。
2. 校验 `vercel.json` 与项目设置。
3. 起隔离构建 VM（预装 Node/Python/Ruby/Go，构建后销毁）。
4. 按 lockfile 装依赖（有层缓存）。
5. 执行构建，产出 Build Output。
6. 上传产物 → 下发路由 → 分配**不可变**部署 URL。
7. 生产部署再走 alias，域名指向新部署。

因此：**部署 URL 不可变，回滚和 promote 改的是 alias**，不是删掉旧部署。

## 7. 本地与线上命令

| 场景 | 命令 | 说明 |
| --- | --- | --- |
| 安装 | `npm i -g vercel` | 本机实测 65.14.0 |
| 绑定项目 | `vercel link` | 生成 `.vercel/project.json` |
| 本地复现 | `vercel dev` | 起 Functions + Routing Middleware；`--listen <port>` 换端口 |
| 免登录试跑 | `vercel dev --local` | 不拉环境变量、不校验项目链接，配置只读 `vercel.json` |
| 构建 | `vercel build` | **必须已登录**，否则 `The specified token is not valid` |
| 只传产物 | `vercel deploy --prebuilt` | CI 常用 |
| 上线 | `vercel --prod` | 生产部署 + alias |
| 排查 | `vercel inspect <url>`、`vercel logs <url>` | 看函数清单、运行时、日志 |

Windows + 沙箱环境的两个坑（实测）：

- npm 默认缓存在 `AppData` 报 `EPERM`：加 `--cache "$PWD\.npm-cache"`，并 `--ignore-scripts` 规避 postinstall。
- `vercel dev` 会往 `%APPDATA%\com.vercel.cli` 建目录、往 `%LOCALAPPDATA%` 建 symlink，沙箱下报 `EPERM: operation not permitted, symlink ...`；改用 `--global-config` 指向工作区可绕过前一半，symlink 那步绕不过。

## 8. 三个可在本地验证的模式

示例工程 [`deploy/vercel-edge/`](../deploy/vercel-edge/)：

| 文件 | 模式 | 为什么值得学 |
| --- | --- | --- |
| `api/router.js` | 单函数多路由 | 官方不建议「一个端点一个文件」：每个文件都是独立函数，影响 Fluid compute 的实例复用。`vercel.json` 把 `/api/:p*` 改写成 `/api/router?p=:p*`，函数内分派。代价是原 pathname 被改写吞掉，必须靠 query 透传 |
| `api/auth.js` | Web Crypto HMAC | 边缘没有 Node `crypto`；签发 `payload.signature`，校验用逐字节异或累积做等时比较，避免 401 的耗时差泄漏信息 |
| `api/proxy.js` | 反向代理 | 剥掉 `host/connection/content-length`、加 `x-forwarded-host`、`AbortSignal.timeout(8000)` 控上游、异常统一转 502、响应体流式透传 |

验收脚本 `node deploy/vercel-edge/test/run.mjs`（需 `npm i -D @edge-runtime/vm`）：

```
13 passed, 0 failed
```

覆盖：200 带 CORS 与 `s-maxage`、OPTIONS 204、POST 201、坏 JSON 400、方法错误 405 + `allow`、未知路径 404、令牌签发/校验/篡改 401、代理透传 query 与 body、上游异常 502。三个文件 gzip 合计 2.1 KB，相对 Hobby 的 1 MB 上限余量约 1650 倍。

## 9. 未验证项（如实标注）

- 未真实部署：缺 `VERCEL_TOKEN`，`vercel build` 直接失败，无产物可传。
- 未在真实边缘节点验证区域调度与冷启动表现。
- `@edge-runtime/vm` 是本地等价语义，不等于线上行为；CORS、缓存头、流式行为在 CDN 层可能还有差异。

补上 token 后的验收口径：`curl -s https://<url>/api/router?p=info` 返回 200 且 `"edge": true`；`vercel inspect` 中该函数 runtime 显示 `edge`；Dashboard → Functions 能看到它。

## 10. 参考资料

- 边缘运行时：<https://vercel.com/docs/functions/runtimes/edge>
- 函数限制：<https://vercel.com/docs/functions/limitations>
- Build Output API：<https://vercel.com/docs/build-output-api/primitives>
- 项目配置：<https://vercel.com/docs/project-configuration/vercel-json>
- CLI：<https://vercel.com/docs/cli>
