add_executable(l2mapconv_tests
    TestMain.cpp
    MapCatalogTests.cpp
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
    StaticMeshCollisionTests.cpp
    RecentClientsTests.cpp
    ClientStartupTests.cpp
    ClientSessionTests.cpp
    DesktopStartupTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/CameraMotion.cpp
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
target_link_libraries(l2mapconv_tests PRIVATE
    geodata geometry unreal rendering territory utils
    glfw libglew_static glm imgui cxxopts Recast
)

if(L2MAPCONV_LOAD_TEXTURES)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TEXTURES)
endif()
if(L2MAPCONV_LOAD_TERRAIN)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TERRAIN)
endif()

add_test(NAME l2mapconv_tests COMMAND l2mapconv_tests)

add_executable(live_visual_tests LiveVisualRendererTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/LiveVisualRenderer.cpp
    ${CMAKE_SOURCE_DIR}/application/src/LiveVisualShaders.cpp)
target_include_directories(live_visual_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src)
target_link_libraries(live_visual_tests PRIVATE
    territory rendering geometry unreal utils glfw libglew_static glm)
set_target_properties(live_visual_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(live_visual_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
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
target_link_libraries(window_system_tests PRIVATE
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
