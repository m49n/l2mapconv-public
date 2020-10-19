cmake_minimum_required(VERSION 3.17)
project(l2mapconv)

add_executable(${PROJECT_NAME}
    src/pch.cpp
    src/main.cpp

    src/Application.cpp
    src/WindowSystem.cpp
    src/RenderingSystem.cpp
    src/UISystem.cpp
    src/CameraSystem.cpp
    src/LoadingSystem.cpp

    src/UnrealLoader.cpp
    src/GeodataLoader.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PRIVATE utils
    PRIVATE math
    PRIVATE unreal
    PRIVATE rendering
    PRIVATE geodata

    PRIVATE glfw
    PRIVATE libglew_static
    PRIVATE glm
    PRIVATE imgui
)

# Compiler options
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

# Copy resources
add_custom_target(copy-resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
)
