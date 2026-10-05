add_executable(navmesh_tests NavmeshTests.cpp)
add_executable(navmesh_world_tests NavmeshWorldTests.cpp)
target_link_libraries(navmesh_world_tests PRIVATE navmesh)
set_target_properties(navmesh_world_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(navmesh_world_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
add_test(NAME navmesh_world_tests COMMAND navmesh_world_tests)
add_executable(navmesh_world_probe navmesh/WorldProbe.cpp)
target_link_libraries(navmesh_world_probe PRIVATE pathfinding)
set_target_properties(navmesh_world_probe PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(navmesh_world_probe PRIVATE ${TARGET_COMPILE_OPTIONS})
add_executable(pathfinding_tests pathfinding/Main.cpp pathfinding/ContractTests.cpp
    pathfinding/DatasetTests.cpp pathfinding/ViewTests.cpp
    pathfinding/BackendAdapterTests.cpp
    pathfinding/ProcessTests.cpp
    pathfinding/JobTests.cpp
    pathfinding/ControllerTests.cpp
    pathfinding/InputTests.cpp
    pathfinding/OverlayTests.cpp
    pathfinding/ResultPresentationTests.cpp
    pathfinding/ReplayTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingLoader.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingOverlay.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingResults.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ClientFolderPicker.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingJob.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingWorker.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingController.cpp
    ${CMAKE_SOURCE_DIR}/application/src/OwnedChildProcess.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryProcess.cpp
    ${CMAKE_SOURCE_DIR}/application/src/PathfindingView.cpp)
target_include_directories(pathfinding_tests PRIVATE ${CMAKE_SOURCE_DIR}/application/src)
target_link_libraries(pathfinding_tests PRIVATE pathfinding rendering imgui)
target_include_directories(pathfinding_tests SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs/imgui/imgui)
if(WIN32)
    target_link_libraries(pathfinding_tests PRIVATE ole32 shell32 uuid)
endif()
set_target_properties(pathfinding_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(pathfinding_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
add_test(NAME pathfinding_tests COMMAND pathfinding_tests)
add_executable(pathfinding_process_child pathfinding/ProcessChild.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryProcess.cpp)
target_include_directories(pathfinding_process_child PRIVATE ${CMAKE_SOURCE_DIR}/application/src)
target_link_libraries(pathfinding_process_child PRIVATE territory)
set_target_properties(pathfinding_process_child PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(pathfinding_process_child PRIVATE ${TARGET_COMPILE_OPTIONS})
add_dependencies(pathfinding_tests pathfinding_process_child)
add_dependencies(pathfinding_tests l2mapconv)
target_link_libraries(navmesh_tests PRIVATE navmesh)
set_target_properties(navmesh_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(navmesh_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
add_test(NAME navmesh_tests COMMAND navmesh_tests)

add_executable(l2mapconv_tests
    TestMain.cpp
    CameraSystemTests.cpp
    MapCatalogTests.cpp
    MapNavigationTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapNavigation.cpp
    MapResidencyTests.cpp
    MapLoadOptionsTests.cpp
    SceneGroupTests.cpp
    LiveSceneSettingsTests.cpp
    MapSelectionTests.cpp
    MapLoadingTests.cpp
    MapStreamingTests.cpp
    SystemStackTests.cpp
    ImportedGeodataTests.cpp
    PtsGeodataTests.cpp
    GeodataGenerationTests.cpp
    GeodataBuildTests.cpp
    NavmeshRegionGeometryTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/NavmeshRegionGeometry.cpp
    ${CMAKE_SOURCE_DIR}/application/src/GeodataBuildJob.cpp
    ${CMAKE_SOURCE_DIR}/application/src/GeodataBuildController.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryRenderPolicy.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryProcess.cpp
    StaticMeshCollisionTests.cpp
    RecentClientsTests.cpp
    ClientStartupTests.cpp
    ClientSessionTests.cpp
    DesktopStartupTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/CameraMotion.cpp
    ${CMAKE_SOURCE_DIR}/application/src/CameraSystem.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ExecutablePath.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapCatalog.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapGridTransform.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapResidency.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapSelectionContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapLoadQueue.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapLoadingWorker.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapStreamingSystem.cpp
    ${CMAKE_SOURCE_DIR}/application/src/SystemStack.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ImportedGeodataLoader.cpp
    ${CMAKE_SOURCE_DIR}/application/src/RecentClients.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ClientStartup.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ClientSessionContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/DesktopStartup.cpp
    ${CMAKE_SOURCE_DIR}/application/src/UnrealLoader.cpp
)

set_target_properties(l2mapconv_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(l2mapconv_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
target_include_directories(l2mapconv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src
    ${CMAKE_SOURCE_DIR}/geodata/src
)
target_include_directories(l2mapconv_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui
)
target_link_libraries(l2mapconv_tests PRIVATE
    geodata navmesh geometry unreal rendering territory utils
    glfw libglew_static glm imgui cxxopts Recast
)

if(L2MAPCONV_LOAD_TEXTURES)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TEXTURES)
endif()
if(L2MAPCONV_LOAD_TERRAIN)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TERRAIN)
endif()

add_test(NAME l2mapconv_tests COMMAND l2mapconv_tests)
add_dependencies(l2mapconv_tests l2mapconv)
add_test(NAME geodata_job_cli COMMAND ${CMAKE_COMMAND}
    "-DAPP=$<TARGET_FILE:l2mapconv>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/GeodataJobCliTests.cmake")

add_executable(ui_settings_tests UISettingsTests.cpp)
target_include_directories(ui_settings_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src)
target_include_directories(ui_settings_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui)
if(WIN32)
    target_compile_definitions(ui_settings_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
target_link_libraries(ui_settings_tests PRIVATE imgui)
set_target_properties(ui_settings_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(ui_settings_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
add_test(NAME ui_settings_tests COMMAND ui_settings_tests)

add_executable(live_visual_tests LiveVisualRendererTests.cpp LiveClientCapture.cpp
    ManualTerrainRendererTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/application/src/RendererMapSceneSink.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ImportedGeodataLoader.cpp
    ${CMAKE_SOURCE_DIR}/application/src/UnrealLoader.cpp
    ${CMAKE_SOURCE_DIR}/application/src/LiveVisualRenderer.cpp
    ${CMAKE_SOURCE_DIR}/application/src/LiveVisualShaders.cpp)
target_include_directories(live_visual_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src)
target_link_libraries(live_visual_tests PRIVATE
    territory rendering geometry unreal geodata navmesh utils glfw libglew_static glm imgui cxxopts)
target_include_directories(live_visual_tests SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/imgui)
target_compile_definitions(live_visual_tests PRIVATE
    L2MAPCONV_TEST_RESOURCES="${CMAKE_SOURCE_DIR}/application/install")
if(L2MAPCONV_LOAD_TERRAIN)
    target_compile_definitions(live_visual_tests PRIVATE LOAD_TERRAIN)
endif()
set_target_properties(live_visual_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(live_visual_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
target_precompile_headers(live_visual_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src/pch.h)
add_test(NAME live_visual_tests COMMAND live_visual_tests)

add_executable(window_system_tests WindowSystemTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/WindowSystem.cpp)
target_precompile_headers(window_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src/pch.h)
set_target_properties(window_system_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(window_system_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
# Use platform headers without linking GLFW or inheriting GLFW_DLL: the test
# supplies that external boundary itself, including in shared-library builds.
target_include_directories(window_system_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src
    ${CMAKE_SOURCE_DIR}/libs/glfw/include
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui)
target_link_libraries(window_system_tests PRIVATE navmesh
    utils geometry unreal rendering geodata libglew_static glm cxxopts)
add_test(NAME window_system_tests COMMAND window_system_tests)

add_test(NAME build_identity COMMAND ${CMAKE_COMMAND}
    "-DAPP=$<TARGET_FILE:l2mapconv>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildIdentityTests.cmake")
add_test(NAME runtime_distribution COMMAND ${CMAKE_COMMAND}
    "-DAPP=$<TARGET_FILE:l2mapconv>" "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}" "-DCONFIG=$<CONFIG>"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}"
    "-DOBJDUMP=${CMAKE_OBJDUMP}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/DistributionTests.cmake")

add_subdirectory(territory)

set(L2MAPCONV_TEST_CLIENT_ROOT "" CACHE PATH "Optional Samurai Crow P542 client for generation integration tests")
if(L2MAPCONV_TEST_CLIENT_ROOT)
    foreach(region 24_18 25_19)
        add_test(NAME map_collision_${region} COMMAND l2mapconv_tests
            --check-map-collisions "${L2MAPCONV_TEST_CLIENT_ROOT}" ${region})
        set_tests_properties(map_collision_${region} PROPERTIES LABELS integration)
    endforeach()
    add_test(NAME build_failure_diagnostic COMMAND ${CMAKE_COMMAND}
        "-DAPP=$<TARGET_FILE:l2mapconv>"
        "-DCLIENT=${L2MAPCONV_TEST_CLIENT_ROOT}"
        "-DFIXTURE=${CMAKE_CURRENT_BINARY_DIR}/build-error-fixture"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/BuildFailureTests.cmake")
    set_tests_properties(build_failure_diagnostic PROPERTIES LABELS integration)
endif()
