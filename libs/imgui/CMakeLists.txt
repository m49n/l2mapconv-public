cmake_minimum_required(VERSION 3.17)
project(imgui)

add_library(${PROJECT_NAME}
    src/imgui.cpp
    src/imgui_demo.cpp
    src/imgui_draw.cpp
    src/imgui_widgets.cpp
    src/imgui_impl_glfw.cpp
    src/imgui_impl_opengl3.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE include/imgui
    PUBLIC include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE glfw
    PRIVATE libglew_static
)
