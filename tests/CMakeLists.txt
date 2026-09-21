add_executable(l2mapconv_tests
    TestMain.cpp
    MapCatalogTests.cpp
    MapResidencyTests.cpp
    MapLoadOptionsTests.cpp
    SceneGroupTests.cpp
    MapSelectionTests.cpp
    MapLoadingTests.cpp
    MapStreamingTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/CameraMotion.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ExecutablePath.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapCatalog.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapGridTransform.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapResidency.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapSelectionContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapLoadQueue.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapLoadingWorker.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapStreamingSystem.cpp
    ${CMAKE_SOURCE_DIR}/application/src/UnrealLoader.cpp
)

set_target_properties(l2mapconv_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(l2mapconv_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
target_include_directories(l2mapconv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src
)
target_link_libraries(l2mapconv_tests PRIVATE
    geodata geometry unreal rendering utils
    glfw libglew_static glm imgui cxxopts
)

if(L2MAPCONV_LOAD_TEXTURES)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TEXTURES)
endif()
if(L2MAPCONV_LOAD_TERRAIN)
    target_compile_definitions(l2mapconv_tests PRIVATE LOAD_TERRAIN)
endif()

add_test(NAME l2mapconv_tests COMMAND l2mapconv_tests)
