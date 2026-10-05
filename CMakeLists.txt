cmake_minimum_required(VERSION 3.22)
project(l2mapconv)

include(CTest)

# Install directory
set(INSTALL_DIR ${CMAKE_BINARY_DIR}/install)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${INSTALL_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DIR})

# Target compile options
if (CMAKE_CXX_COMPILER_ID STREQUAL GNU OR
    CMAKE_CXX_COMPILER_ID STREQUAL Clang OR
    CMAKE_CXX_COMPILER_ID STREQUAL AppleClang)

    set(TARGET_COMPILE_OPTIONS -Wall -Wextra -Werror)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL MSVC)

    set(TARGET_COMPILE_OPTIONS /Wall /WX)
endif()

# Target properties
set(TARGET_PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Modules
add_subdirectory(libs)

add_subdirectory(utils)
add_subdirectory(geometry)
add_subdirectory(unreal)
add_subdirectory(rendering)
add_subdirectory(geodata)
add_subdirectory(navmesh)
add_subdirectory(territory)
add_subdirectory(pathfinding)

add_subdirectory(application)

if(BUILD_TESTING)
    add_subdirectory(tests)

    # Publishing always rebuilds and tests; `cmake --install` alone does neither.
    add_custom_target(publish
        COMMAND ${CMAKE_CTEST_COMMAND} --test-dir "${CMAKE_BINARY_DIR}"
            -C $<CONFIG> --output-on-failure
        COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
            --config $<CONFIG> --prefix "${CMAKE_SOURCE_DIR}/dist" --component Runtime
        DEPENDS copy-resources l2mapconv_tests navmesh_tests pathfinding_tests window_system_tests ui_settings_tests
            live_visual_tests territory_tests territory_png_compare
        USES_TERMINAL
        VERBATIM)
endif()
