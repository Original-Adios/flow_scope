cmake_minimum_required(VERSION 3.14)
project(flow_scope VERSION 0.3.0 LANGUAGES CXX C) # 添加 C 语言支持

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 依赖库 ---
find_package(PkgConfig REQUIRED)
# 查找 libbpf (需先 apt install libbpf-dev)
pkg_check_modules(LIBBPF REQUIRED libbpf)

# 手动包含 third_party
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include_directories(${LIBBPF_INCLUDE_DIRS})

# --- eBPF 编译逻辑 ---
# 1. 定义工具路径
find_program(CLANG_EXE clang REQUIRED)
find_program(BPFTOOL_EXE bpftool REQUIRED)

# 2. 定义文件名
set(BPF_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/bpf/tcp_loss.bpf.c")
set(BPF_OBJ "${CMAKE_CURRENT_BINARY_DIR}/tcp_loss.bpf.o")
set(BPF_SKEL "${CMAKE_CURRENT_BINARY_DIR}/tcp_loss.skel.h")

# 3. 编译: .c -> .o (目标架构 bpf)
# -g: 生成调试信息 (BTF 需要)
# -O2: BPF 必须开启优化
# -target bpf: 目标架构
add_custom_command(OUTPUT ${BPF_OBJ}
    COMMAND ${CLANG_EXE} -g -O2 -target bpf -D__TARGET_ARCH_x86 
            -I/usr/include/x86_64-linux-gnu 
            -I${CMAKE_CURRENT_SOURCE_DIR}/src/bpf
            -c ${BPF_SRC} -o ${BPF_OBJ}
    DEPENDS ${BPF_SRC}
    COMMENT "Compiling eBPF program: ${BPF_SRC}"
)

# 4. 生成 Skeleton: .o -> .skel.h
add_custom_command(OUTPUT ${BPF_SKEL}
    COMMAND ${BPFTOOL_EXE} gen skeleton ${BPF_OBJ} > ${BPF_SKEL}
    DEPENDS ${BPF_OBJ}
    COMMENT "Generating skeleton header: ${BPF_SKEL}"
)

# 定义一个目标，方便主程序依赖
add_custom_target(generate_bpf_skel DEPENDS ${BPF_SKEL})

# --- 主程序 ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(flow_scope ${SOURCES})

# 依赖 BPF 生成步骤
add_dependencies(flow_scope generate_bpf_skel)

# 包含生成的 .skel.h 路径
target_include_directories(flow_scope PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# 链接 libbpf 和 pthread
target_link_libraries(flow_scope PRIVATE ${LIBBPF_LIBRARIES} pthread z elf)