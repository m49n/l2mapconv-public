cmake_minimum_required(VERSION 3.22)
project(l2mapconv)

add_executable(${PROJECT_NAME}
    src/pch.cpp
    src/main.cpp

    src/Application.cpp
    src/WindowSystem.cpp
    src/RenderingSystem.cpp
    src/UISystem.cpp
    src/CameraSystem.cpp
    src/CameraMotion.cpp
    src/ExecutablePath.cpp
    src/MapCatalog.cpp
    src/MapGridTransform.cpp
    src/MapResidency.cpp
    src/MapSelectionContext.cpp
    src/MapLoadQueue.cpp
    src/MapLoadingWorker.cpp
    src/MapStreamingSystem.cpp
    src/RendererMapSceneSink.cpp
    src/UnrealMapSource.cpp
    src/LoadingSystem.cpp
    src/GeodataSystem.cpp

    src/UnrealLoader.cpp
    src/GeodataEntityFactory.cpp

    src/Renderer.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PRIVATE utils
    PRIVATE geometry
    PRIVATE unreal
    PRIVATE rendering
    PRIVATE geodata

    PRIVATE glfw
    PRIVATE libglew_static
    PRIVATE glm
    PRIVATE imgui
    PRIVATE cxxopts
)

# Compiler settings
set_target_properties(${PROJECT_NAME} PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${TARGET_COMPILE_OPTIONS})

# Copy resources
add_custom_target(copy-resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/install
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
)

# CMake options
option(L2MAPCONV_LOAD_TEXTURES "Load Textures" OFF)
if(L2MAPCONV_LOAD_TEXTURES)
  add_definitions(-DLOAD_TEXTURES)
endif()

option(L2MAPCONV_LOAD_TERRAIN "Load Terrain" ON)
if(L2MAPCONV_LOAD_TERRAIN)
  add_definitions(-DLOAD_TERRAIN)
endif()
