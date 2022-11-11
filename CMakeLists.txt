cmake_minimum_required(VERSION 3.0.0)

project(metafs
    VERSION 0.1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

MESSAGE(STATUS "Project: SYSCALL-HOOK")

set(CMAKE_CXX_FLAGS "-std=c++11 -DERPC_INFINIBAND=true -D_FILE_OFFSET_BITS=64 -fpic -O2")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

set(HOOK_HEAD_LIST
    ${PROJECT_SOURCE_DIR}
)

file(GLOB HOOK_SRC
    ${PROJECT_SOURCE_DIR}/demo/*.cc
)

file(GLOB TEST_SRC
    ${PROJECT_SOURCE_DIR}/test/*.cc
)

add_library(demo_hook SHARED ${HOOK_SRC})
add_executable(test ${TEST_SRC})

target_link_libraries(demo_hook
    pthread syscall_intercept
)

target_link_libraries(test
    pthread syscall_intercept
)

target_include_directories(demo_hook PUBLIC ${HOOK_HEAD_LIST})
