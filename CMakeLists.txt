cmake_minimum_required(VERSION 3.7)
project(7DRLx18 VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIVE_PP False)

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

file(GLOB_RECURSE SRC_7DRLx18
    "src/*.cpp"
    "src/*.h"
    "src/*.c"
    "src/*.hpp"
)


# Create executable
add_executable(${PROJECT_NAME}
    ${SRC_7DRLx18}
    ${SRC_Generated}
)


# Includes

include_directories(SYSTEM ${backward_SOURCE_DIR})
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/generated_src)

if (LIVE_PP)
    include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/LivePP/API)
endif()


# Linked libraries

target_link_libraries(${PROJECT_NAME} PRIVATE chocolate)


# Compiler warnings & definitions

target_compile_definitions(${PROJECT_NAME} PRIVATE _USE_MATH_DEFINES)

if (CMAKE_COMPILER_IS_GNUCXX)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -Wno-unknown-pragmas -Wno-gnu-zero-variadic-macro-arguments)
endif()

if (MSVC)
    source_group("src" FILES ${SRC_7DRLx18})
    source_group("foundation" FILES ${bitsquidfoundation_SOURCE_DIR})
    source_group("generated" FILES ${SRC_Generated})
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    set_source_files_properties(${SRC_7DRLx18} PROPERTIES COMPILE_FLAGS "/W4")

    if (LIVE_PP)
        target_compile_definitions(${PROJECT_NAME} PRIVATE LIVE_PP=1)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi /hotpatch /Gm-)
        target_link_options(${PROJECT_NAME} PRIVATE /FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF)
    endif()
endif()

# Enable tests

enable_testing()
add_subdirectory(tests)
