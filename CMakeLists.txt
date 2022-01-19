cmake_minimum_required(VERSION 3.7)
project(7drl18 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)


# Find locally installed dependencies. Tip: Use VCPKG for these.

# Fetch dependencies from Github

include(FetchContent)

FetchContent_Declare(
    BitSquidFoundation
    GIT_REPOSITORY https://github.com/hsjunnesson/bitsquid-foundation.git
)

if (NOT bitsquidfoundation_POPULATED)
    FetchContent_MakeAvailable(BitSquidFoundation)
endif()


add_subdirectory("${CMAKE_SOURCE_DIR}/chocolate")


# Generated protobuf implementation

file(GLOB_RECURSE SRC_Generated
    "generated_src/*.cc"
    "generated_src/*.h"
)


# Main game source

file(GLOB_RECURSE SRC_7drl18
    "src/*.cpp"
    "src/*.h"
    "src/*.c"
    "src/*.hpp"
)


# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_7drl18}
    ${SRC_Generated}
)


# Includes

include_directories(SYSTEM ${backward_SOURCE_DIR})
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/generated_src)


# Linked libraries

target_link_libraries(${PROJECT_NAME} PRIVATE chocolate)


# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Wno-unknown-pragmas -Wno-gnu-zero-variadic-macro-arguments)


# Enable tests

enable_testing()
add_subdirectory(tests)
