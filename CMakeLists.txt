cmake_minimum_required(VERSION 3.14)
project(flow_scope VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O3 -pthread)

# --- 源码文件 ---
file(GLOB_RECURSE SOURCES "src/*.cpp")

# --- 可执行文件 ---
add_executable(flow_scope ${SOURCES})

# --- 包含路径 ---
# 将 src 和 third_party 加入包含路径
target_include_directories(flow_scope PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# --- 链接依赖 ---
# 因为是 header-only 库，不再需要链接 httplib::httplib 这种 target
# 但 httplib 依赖 pthread (Linux下)，OpenSSL (如果是 HTTPS，目前我们只用 HTTP，暂不需要ssl)
target_link_libraries(flow_scope PRIVATE pthread)