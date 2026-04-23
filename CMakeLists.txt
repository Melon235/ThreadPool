cmake_minimum_required(VERSION 3.16)

project(threadpool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(THREADPOOL_BUILD_TESTS "Build test executables" ON)
option(THREADPOOL_BUILD_BENCHMARKS "Build benchmark executables" ON)

find_package(Threads REQUIRED)

set(THREADPOOL_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include")
set(THREADPOOL_POLICY_DIR "${PROJECT_SOURCE_DIR}/policies")
set(THREADPOOL_SRC_DIR "${PROJECT_SOURCE_DIR}/src")
set(THREADPOOL_TEST_DIR "${PROJECT_SOURCE_DIR}/tests")
set(THREADPOOL_BENCHMARK_DIR "${PROJECT_SOURCE_DIR}/benchmarks")

file(GLOB THREADPOOL_SOURCES CONFIGURE_DEPENDS
    "${THREADPOOL_SRC_DIR}/*.cpp"
    "${THREADPOOL_POLICY_DIR}/*.cpp"
)

add_library(thread_pool STATIC ${THREADPOOL_SOURCES})
add_library(thread_pool::thread_pool ALIAS thread_pool)

target_include_directories(thread_pool
    PUBLIC
        "${THREADPOOL_INCLUDE_DIR}"
        "${THREADPOOL_POLICY_DIR}"
)

target_link_libraries(thread_pool
    PUBLIC
        Threads::Threads
)

target_compile_features(thread_pool PUBLIC cxx_std_17)

set_target_properties(thread_pool PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/lib"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/lib"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/bin"
)

function(threadpool_add_executable source_file output_subdir)
    get_filename_component(target_name "${source_file}" NAME_WE)

    add_executable("${target_name}" "${source_file}")
    target_link_libraries("${target_name}" PRIVATE thread_pool)

    set_target_properties("${target_name}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/build/${output_subdir}"
    )
endfunction()

if(THREADPOOL_BUILD_TESTS)
    enable_testing()

    file(GLOB THREADPOOL_TEST_SOURCES CONFIGURE_DEPENDS
        "${THREADPOOL_TEST_DIR}/*.cpp"
    )

    foreach(test_source IN LISTS THREADPOOL_TEST_SOURCES)
        get_filename_component(test_name "${test_source}" NAME_WE)
        threadpool_add_executable("${test_source}" "tests")
        add_test(NAME "${test_name}" COMMAND "${test_name}")
        set_tests_properties("${test_name}" PROPERTIES
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        )
    endforeach()
endif()

if(THREADPOOL_BUILD_BENCHMARKS)
    file(GLOB THREADPOOL_BENCHMARK_SOURCES CONFIGURE_DEPENDS
        "${THREADPOOL_BENCHMARK_DIR}/*.cpp"
    )

    foreach(benchmark_source IN LISTS THREADPOOL_BENCHMARK_SOURCES)
        if(EXISTS "${benchmark_source}" AND NOT IS_DIRECTORY "${benchmark_source}")
            file(SIZE "${benchmark_source}" benchmark_size)
            if(benchmark_size GREATER 0)
                threadpool_add_executable("${benchmark_source}" "benchmarks")
            endif()
        endif()
    endforeach()
endif()
