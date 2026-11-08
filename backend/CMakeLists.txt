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

# nlohmann/json 3.11.3
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Crow 1.2.0：只启用 HTTP（关闭 SSL/压缩，避免 OpenSSL/zlib 依赖）
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CROW_FEATURES "" CACHE STRING "" FORCE)
FetchContent_Declare(Crow
  GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
  GIT_TAG v1.2.0
)
FetchContent_MakeAvailable(Crow)

# GoogleTest 1.15.2
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Boost（Crow 依赖 Boost.Asio/System）与线程库
find_package(Boost REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# ---- 核心静态库：domain + control + api + config + dto ----
add_library(patrol_core STATIC
  src/domain/grid.cpp
  src/domain/robot.cpp
  src/domain/patrol_planner.cpp
  src/control/robot_controller.cpp
  src/config.cpp
  src/api/command_parser.cpp
  src/api/http_server.cpp
)
target_include_directories(patrol_core PUBLIC src)
target_link_libraries(patrol_core PUBLIC
  nlohmann_json::nlohmann_json
  Crow::Crow
  Boost::system
  Threads::Threads
)

# ---- 可执行文件 ----
add_executable(patrol_server src/main.cpp)
target_link_libraries(patrol_server PRIVATE patrol_core)

# ---- 单元测试 ----
enable_testing()
add_executable(domain_tests
  tests/domain_tests.cpp
  tests/controller_tests.cpp
  tests/command_parser_tests.cpp
  tests/patrol_planner_tests.cpp
)
target_link_libraries(domain_tests PRIVATE patrol_core GTest::gtest_main)
add_test(NAME domain_tests COMMAND domain_tests)
