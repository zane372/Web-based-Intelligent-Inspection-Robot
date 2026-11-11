cmake_minimum_required(VERSION 3.24)
project(patrol_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# ---- 依赖（全部固定 release 锁定，见 SPEC 5.6）----

# nlohmann/json 3.11.3（固定到 tag 对应 commit SHA，防可变 tag 被改写，见 SPEC 7 供应链）
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 # v3.11.3
)
FetchContent_MakeAvailable(json)

# Crow 1.2.0：只启用 HTTP（关闭 SSL/压缩，避免 OpenSSL/zlib 依赖）
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_FEATURES "" CACHE STRING "" FORCE)
FetchContent_Declare(Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG 93bd8fa05d440383037f43803380ee58400fb8da # v1.2.0
  # 请求体缓冲上限补丁（防超大 body 在进入 handler 前耗尽内存，见 SPEC 7 防御边界）。
  # 注意：PATCH_COMMAND 仅在 FetchContent 首次下载时执行；若变更补丁内容，
  # 需删除 build/_deps/crow-src 让 CMake 重新应用，否则会复用旧的未打补丁源。
  PATCH_COMMAND git apply "${CMAKE_SOURCE_DIR}/patches/crow-body-limit.patch"
)
FetchContent_MakeAvailable(Crow)

# GoogleTest 1.15.2（固定到 tag 对应 commit SHA）
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59 # v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Boost（Crow 依赖 Boost.Asio/System）与线程库
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# ---- 依赖分层（SPEC 5.4）：
#   patrol_domain     -> 仅 C++ 标准库
#   patrol_control    -> patrol_domain（+ Threads）
#   patrol_perception -> patrol_domain（M2 加入）
#   patrol_api        -> patrol_control + patrol_perception + Crow + json + Boost
#   patrol_server     -> patrol_api + 配置加载 + 依赖组装
# ----

# patrol_domain：地图、机器人、巡检规划。链接依赖中不得出现 Crow/Boost/json。
add_library(patrol_domain STATIC
  src/domain/grid.cpp
  src/domain/robot.cpp
  src/domain/patrol_planner.cpp
)
target_include_directories(patrol_domain PUBLIC src)

# patrol_control：机器人控制与巡检执行。仅标准库 + Threads。
add_library(patrol_control STATIC
  src/control/patrol_executor.cpp
  src/control/robot_controller.cpp
)
target_include_directories(patrol_control PUBLIC src)
target_link_libraries(patrol_control PUBLIC
  patrol_domain
  Threads::Threads
)

# patrol_perception：虚拟感知（规则纯函数 + 观察/事件服务）。仅标准库。
add_library(patrol_perception STATIC
  src/perception/perception_service.cpp
)
target_include_directories(patrol_perception PUBLIC src)
target_link_libraries(patrol_perception PUBLIC patrol_domain)

# patrol_api：DTO、配置、命令解析与 HTTP 路由。
add_library(patrol_api STATIC
  src/config.cpp
  src/api/command_parser.cpp
  src/api/http_server.cpp
)
target_include_directories(patrol_api PUBLIC src)
target_link_libraries(patrol_api PUBLIC
  patrol_control
  patrol_perception
  nlohmann_json::nlohmann_json
  Crow::Crow
  Boost::system
)

# ---- 可执行文件 ----
add_executable(patrol_server src/main.cpp)
target_link_libraries(patrol_server PRIVATE patrol_api)

# ---- 单元测试 ----
enable_testing()
add_executable(domain_tests
  tests/domain_tests.cpp
  tests/controller_tests.cpp
  tests/command_parser_tests.cpp
  tests/patrol_planner_tests.cpp
  tests/config_tests.cpp
  tests/path_safety_tests.cpp
)
target_link_libraries(domain_tests PRIVATE patrol_api GTest::gtest_main)
add_test(NAME domain_tests COMMAND domain_tests)

# 感知纯规则与服务测试：仅依赖 patrol_perception（+ GoogleTest），
# 不启动 HTTP、不依赖 Crow/Boost（SPEC 5.4 / 5.8）。
add_executable(perception_tests
  tests/perception_tests.cpp
)
target_link_libraries(perception_tests PRIVATE patrol_perception GTest::gtest_main)
add_test(NAME perception_tests COMMAND perception_tests)
