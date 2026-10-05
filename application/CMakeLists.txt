cmake_minimum_required(VERSION 3.22)
project(l2mapconv)

add_executable(${PROJECT_NAME}
    src/pch.cpp
    src/main.cpp

    src/Application.cpp
    src/WindowSystem.cpp
    src/RenderingSystem.cpp
    src/LiveVisualRenderer.cpp
    src/LiveVisualShaders.cpp
    src/UISystem.cpp
    src/CameraSystem.cpp
    src/CameraMotion.cpp
    src/SystemStack.cpp
    src/ExecutablePath.cpp
    src/MapCatalog.cpp
    src/MapGridTransform.cpp
    src/MapNavigation.cpp
    src/OwnedChildProcess.cpp
    src/PathfindingJob.cpp
    src/PathfindingWorker.cpp
    src/PathfindingController.cpp
    src/PathfindingView.cpp
    src/PathfindingContext.cpp
    src/PathfindingLoader.cpp
    src/PathfindingOverlay.cpp
    src/PathfindingWindow.cpp
    src/PathfindingResults.cpp
    src/MapResidency.cpp
    src/MapSelectionContext.cpp
    src/MapLoadQueue.cpp
    src/MapLoadingWorker.cpp
    src/MapStreamingSystem.cpp
    src/RendererMapSceneSink.cpp
    src/ImportedGeodataLoader.cpp
    src/RecentClients.cpp
    src/ClientStartup.cpp
    src/ClientFolderPicker.cpp
    src/ClientSessionContext.cpp
    src/DesktopStartup.cpp
    src/UnrealMapSource.cpp
    src/LoadingSystem.cpp
    src/GeodataSystem.cpp
    src/GeodataBuildJob.cpp
    src/GeodataBuildController.cpp
    src/GeodataBuildWorker.cpp
    src/NavmeshRegionGeometry.cpp

    src/UnrealLoader.cpp
    src/GeodataEntityFactory.cpp

    src/Renderer.cpp
    src/TerritoryProcess.cpp
    src/TerritoryRenderController.cpp
    src/TerritoryCommandLine.cpp
    src/TerritoryRenderPolicy.cpp
    src/TerritoryRenderWindow.cpp
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PRIVATE utils
    PRIVATE geometry
    PRIVATE unreal
    PRIVATE rendering
    PRIVATE geodata
    PRIVATE navmesh
    PRIVATE territory
    PRIVATE pathfinding

    PRIVATE glfw
    PRIVATE libglew_static
    PRIVATE glm
    PRIVATE imgui
    PRIVATE cxxopts
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 shell32 uuid)
endif()

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

# One application implements desktop preview, geodata building and radar CLI/UI.
set(BUILD_IDENTITY_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildIdentity.h")
add_custom_target(build-identity
    COMMAND ${CMAKE_COMMAND}
        "-DGIT=${GIT_EXECUTABLE}" "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DOUTPUT=${BUILD_IDENTITY_HEADER}" "-DLOAD_TEXTURES=${L2MAPCONV_LOAD_TEXTURES}"
        -P "${CMAKE_SOURCE_DIR}/cmake/BuildIdentity.cmake"
    BYPRODUCTS "${BUILD_IDENTITY_HEADER}"
    VERBATIM)
add_dependencies(${PROJECT_NAME} build-identity)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Package source resources only, never developer test EXEs or generated outputs.
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . COMPONENT Runtime)
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:l2mapconv> DESTINATION . COMPONENT Runtime)
    # Compiler runtime DLLs are not CMake link targets. Use the toolchain's
    # redistributables, not copies from System32 or an installer on the host.
    include("${CMAKE_SOURCE_DIR}/cmake/RuntimeLibraries.cmake")
endif()
install(DIRECTORY install/ DESTINATION . COMPONENT Runtime)
set(L2MAPCONV_PATHFINDING_BACKEND_DIR "" CACHE PATH
    "Prepared local Pathfinding Lab backend bundle (never a live server directory)")
if(L2MAPCONV_PATHFINDING_BACKEND_DIR)
    if(NOT EXISTS "${L2MAPCONV_PATHFINDING_BACKEND_DIR}/profile.json" OR
       NOT EXISTS "${L2MAPCONV_PATHFINDING_BACKEND_DIR}/bundle-manifest.json")
        message(FATAL_ERROR "Pathfinding backend must contain profile.json and bundle-manifest.json")
    endif()
    # Do not publish an old Java runner after changing its C++/UI companion.
    install(CODE "
        file(READ \"${L2MAPCONV_PATHFINDING_BACKEND_DIR}/bundle-manifest.json\" manifest)
        string(JSON bundled_source_hash GET \"\$\{manifest\}\" runner_source_sha256)
        file(SHA256 \"${CMAKE_SOURCE_DIR}/tools/pathfinding/NavmeshRouteRunner.java\" source_hash)
        if(NOT source_hash STREQUAL bundled_source_hash)
            message(FATAL_ERROR \"Repackage Pathfinding backend: NavmeshRouteRunner.java changed\")
        endif()
        foreach(source BenchmarkProgress.java)
            string(JSON expected GET \"\$\{manifest\}\" support_sources \"\$\{source\}\")
            file(SHA256 \"${CMAKE_SOURCE_DIR}/tools/pathfinding/\$\{source\}\" actual)
            if(NOT actual STREQUAL expected)
                message(FATAL_ERROR \"Repackage Pathfinding backend: \$\{source\} changed\")
            endif()
        endforeach()
    " COMPONENT Runtime)
    install(DIRECTORY "${L2MAPCONV_PATHFINDING_BACKEND_DIR}/"
        DESTINATION pathfinding-backend COMPONENT Runtime)
endif()
install(DIRECTORY "${CMAKE_SOURCE_DIR}/tools/pathfinding/" DESTINATION tools/pathfinding COMPONENT Runtime
    FILES_MATCHING PATTERN "*.java" PATTERN "*.ps1" PATTERN "*.gradle" PATTERN "*.md")
install(FILES "${CMAKE_SOURCE_DIR}/README.md" "${CMAKE_SOURCE_DIR}/CREDITS.md"
    "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION . COMPONENT Runtime)
