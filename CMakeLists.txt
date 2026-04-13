cmake_minimum_required(VERSION 3.16)
project(ThreadPoolProject LANGUAGES CXX)

# =====================
# 设置 C++ 标准
# =====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =====================
# 包含目录
# =====================
# include/thread_pool 存放公共头文件
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/policies)

# =====================
# 查找源文件
# =====================
# src 下 cpp 文件
file(GLOB_RECURSE SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# policies 下 cpp 文件
file(GLOB_RECURSE POLICY_FILES ${PROJECT_SOURCE_DIR}/policies/*.cpp)

# =====================
# 主可执行文件（线程池测试）
# =====================
#add_executable(threadpool_main
#    ${SRC_FILES}
#    ${POLICY_FILES}
#)

# =====================
# 测试可执行文件
# =====================
# 查找 tests 下 cpp
file(GLOB_RECURSE TEST_FILES ${PROJECT_SOURCE_DIR}/tests/*.cpp)

add_executable(threadpool_tests
    ${SRC_FILES}
    ${POLICY_FILES}
    ${TEST_FILES}
)

# =====================
# 链接线程库（pthread）
# =====================
# target_link_libraries(threadpool_main PRIVATE pthread)
target_link_libraries(threadpool_tests PRIVATE pthread)

# =====================
# 可选：设置输出目录
# =====================
# set_target_properties(threadpool_main PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
# )
set_target_properties(threadpool_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/build/bin
)