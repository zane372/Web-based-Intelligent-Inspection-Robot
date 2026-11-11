# Vercel 边缘函数示例

三个可在本地验证的边缘函数写法，配套说明见 [`docs/vercel-edge.md`](../../docs/vercel-edge.md)。

```
deploy/vercel-edge/
├── package.json          {"type":"module"} —— 非框架项目启用 edge 的硬性前提
├── vercel.json           regions + rewrites + headers
├── api/router.js         单函数多路由：/api/:p* 改写为 /api/router?p=:p*
├── api/auth.js           Web Crypto HMAC 令牌签发与等时校验
├── api/proxy.js          反向代理：剥头、超时、502 兜底、流式透传
└── test/run.mjs          EdgeVM 行为断言（13 条）
```

## 本地验证

无需 Vercel 账号与登录态：

```bash
cd deploy/vercel-edge
npm install --ignore-scripts
npm test
```

预期输出 `13 passed, 0 failed`。断言在 `@edge-runtime/vm` 的 V8 隔离里执行，与线上 Edge Runtime 同语义（无 `fs`、禁 `eval`、`crypto.subtle` 可用）。

## 部署（需已登录）

```bash
npm i -g vercel
vercel link
vercel dev          # 本地复现线上环境
vercel --prod       # 生产部署
```

验收：`curl -s https://<部署域名>/api/router?p=info` 返回 200 且 `"edge": true`；`vercel inspect <url>` 中函数运行时显示 `edge`。

> 本示例尚未真实部署：本机无 `VERCEL_TOKEN`，`vercel build` 报 `The specified token is not valid`。因此仓库内不含任何线上地址。
