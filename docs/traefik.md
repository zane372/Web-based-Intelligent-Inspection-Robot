# Traefik 反向代理部署与配置说明

> 本文说明本项目如何用 [Traefik](https://doc.traefik.io/traefik/) 作为对外反向代理，补充应用层缺失的「用户鉴权」与「请求体大小限制」这两道公网部署必需的安全边界。应用层本身无用户鉴权、无应用层 TLS，因此对外访问一律经 Traefik。

## 1. 为什么用 Traefik

后端是单进程 C++ 服务（Crow），静态前端与 `/api/v1` 同源同端口（容器内 `8080`），本身无用户鉴权、无 TLS。按 SPEC 的公网边界要求，对外暴露必须：

1. 置于**带鉴权**的反向代理之后；
2. 由反代**限制请求体大小**（与后端 4096 字节上限对齐，防御纵深）；
3. 公网 HTTPS 由反代**终结 TLS**。

Traefik 作为云原生边缘路由器，能通过 Docker Provider 自动发现容器 label、动态热加载路由，天然适合本项目的 Docker Compose 交付方式。本文默认启用 HTTP（`80`）+ basicAuth + 请求体限制；HTTPS（ACME）留作可选，见 §6。

## 2. 访问入口

| 入口 | 地址 | 说明 |
| --- | --- | --- |
| 对外应用 | `http://localhost/`（或宿主机 IP） | 经 Traefik，**带 basicAuth**，请求体上限 4096 字节 |
| 管理面板 | `http://127.0.0.1:8082/dashboard/` | Traefik Dashboard，**仅本机回环**，独立 basicAuth |
| 本地直连 | `http://127.0.0.1:8080` | 绕过 Traefik，**无鉴权**，仅绑定宿主机回环，供开发／测试／CI |

默认账号：`test / test`（app 与管理面板各自独立，见 `traefik/dynamic.yml`）。**仅供演示，部署前务必更换**（§5）。

## 3. 架构

```
浏览器 ──► Traefik（:80，basicAuth + 请求体 4096 上限）
              │  路由规则 PathPrefix(`/`)
              ▼
         app 容器（内部网络 traefik-net :8080）
              │
              ▼
         Crow：静态 dist + /api/v1
```

Traefik 与 `app` 同处 `traefik-net` 桥接网络；app 通过 Docker Provider 自动被发现，`127.0.0.1:8080` 的宿主机回环映射仅作本地直连，不影响 Traefik 路由。

## 4. 配置说明

> **Traefik 版本**：使用 `traefik:v3.7.18`。本项目 Docker 引擎为 29.x，其 Docker API 拒绝 `API < 1.40` 的请求（返回 400 Bad Request）；Traefik v3.5 内置的 Docker 客户端请求的 API 版本低于 1.40，会导致 Docker Provider 无法发现容器、对外统一 404。v3.7 起兼容 Docker 29 API，**勿降级到 3.7 以下**。

| 文件 | 类型 | 作用 |
| --- | --- | --- |
| `traefik/traefik.yml` | 静态配置 | 入口点（web/dashboard）、Docker+File Provider、API/Dashboard、日志 |
| `traefik/dynamic.yml` | 动态配置 | 中间件（`body-limit`、`app-auth`、`dashboard-auth`）+ 管理面板路由 |
| `docker-compose.yml` | Compose | `traefik` 服务 + `app` 容器的 `traefik.http.*` label |

- **静态配置**（改动需重启 Traefik）：`entryPoints.web=:80`、`entryPoints.dashboard=:8082`、`providers.docker`（`exposedByDefault=false`，仅发现 `traefik.enable=true` 的容器；不显式指定 network，由 Traefik 自动选择与容器共享的网络）、`providers.file`（读 `dynamic.yml`）、`api.dashboard=true`（secure 模式）、`ping:{}`（供容器 healthcheck）、`global.aliasHeadersStrategy=reject`（防别名头伪装，v3.7 新增）。
- **动态配置**（热加载，无需重启）：`body-limit`（`buffering.maxRequestBodyBytes=4096`）、`app-auth`／`dashboard-auth`（`basicAuth`）、`dashboard` 路由（指向内置 `api@internal`）。
- **app label**：`traefik.http.routers.app.rule=PathPrefix(\`/\`)`、`.entrypoints=web`、`.middlewares=app-auth@file,body-limit@file`、`.services.app.loadbalancer.server.port=8080`。中间件用 `@file` 后缀引用 File Provider 里的定义（跨 Provider 引用语法）。

## 5. 更换鉴权口令

basicAuth 口令以 `htpasswd` 哈希形式写在 `traefik/dynamic.yml`：

```bash
htpasswd -nb patrol '你的强口令'
# 输出形如：patrol:$apr1$xxxxxxxx$yyyyyyyyyyyyyyyyyyyyy
```

把输出的整段 `用户名:哈希` 替换 `dynamic.yml` 中 `app-auth`／`dashboard-auth` 的 `users` 项，然后：

```bash
docker compose up -d --force-recreate traefik   # 或 docker compose restart traefik
```

## 6. 启用 HTTPS（可选，公网必需）

公网 HTTPS 需先有域名与可达的 80/443 端口。在 `traefik/traefik.yml` 增加 `websecure` 入口与 ACME 证书解析器，并在 `docker-compose.yml` 暴露 `443:443`：

```yaml
# traefik/traefik.yml（示意）
entryPoints:
  websecure:
    address: ":443"
certificatesResolvers:
  letsencrypt:
    acme:
      email: "you@example.com"
      storage: "/acme.json"
      httpChallenge:
        entryPoint: web
```

再给 app 路由加 TLS：`traefik.http.routers.app.tls=true`、`traefik.http.routers.app.tls.certresolver=letsencrypt`，并挂载 `./acme.json:/acme.json`（权限 `600`）。证书由 ACME 自动签发与续期。

## 7. 安全注意事项

- **Docker socket**：`docker-compose.yml` 以只读（`:ro`）挂载 `/var/run/docker.sock` 供 Docker Provider 自动发现。只读能降低但不能消除风险；生产环境建议改用 [docker-socket-proxy](https://doc.traefik.io/traefik/providers/docker/#docker-socket-proxy) 收紧权限。
- **防头注入（别名头伪装）**：`global.aliasHeadersStrategy=reject` 会在请求头名含非字母/数字/连字符（如 `X_Auth_User`、`X.Auth.User`）时直接返回 400，防止客户端伪装 Traefik 管理的头（如 basicAuth 结果）。
- **默认口令**：`test / test` 仅演示，公网部署前必须更换（§5）。
- **无应用层 TLS**：`127.0.0.1:8080` 直连仍为明文 HTTP，仅限本机回环；公网统一走 Traefik，务必加 HTTPS（§6）。
- **请求体限制**：反代层 `4096` 上限与后端应用层一致，超限在 Traefik 即返回 `413`，避免超大请求体到达后端。

## 8. 常用操作

```bash
docker compose up --build -d            # 启动（含 Traefik）
docker compose ps                       # 查看 traefik / app 状态
docker compose logs --no-color traefik  # 查看 Traefik 日志
docker compose restart traefik          # 静态配置改动后重启
# 动态配置（dynamic.yml）改动后 Traefik 会自动热加载，无需重启
```
