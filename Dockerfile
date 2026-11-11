# 阶段一：前端构建
FROM node:22-bookworm-slim@sha256:5ec23b7c622b00547753bc6ffd78085dd868a68aeb41c8e0a1418f033e13d96d AS web-build
WORKDIR /src/frontend
COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci
COPY frontend/ ./
RUN npm run build

# 阶段二：C++ 后端构建（Release + CTest）
FROM gcc:12-bookworm@sha256:3d3af69c72edf0652bb3a829965142e81891899c7827d849bc3c0dbb9edd216a AS cpp-build
WORKDIR /src
RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake ninja-build git ca-certificates libboost-system-dev libasio-dev \
    && rm -rf /var/lib/apt/lists/*
COPY backend/ ./backend/
RUN cmake -S backend -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

# 阶段三：精简运行时
FROM debian:bookworm-slim@sha256:734a72b3d6142a71993b4bc580ca01d8ca687f77a2c422d96a961d3376b179bc AS runtime
RUN apt-get update \
    && apt-get install -y --no-install-recommends curl ca-certificates libstdc++6 libboost-system1.74.0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --create-home app
WORKDIR /app
COPY --from=cpp-build /src/build/patrol_server ./patrol_server
COPY --from=web-build /src/frontend/dist ./web
COPY config/ ./config/
USER app
EXPOSE 8080
ENTRYPOINT ["/app/patrol_server"]
