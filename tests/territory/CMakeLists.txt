add_executable(territory_tests TestMain.cpp Fixtures.cpp JobTests.cpp ReferenceTests.cpp TextureTests.cpp MaterialTests.cpp MaterialAudit.cpp GeometryTests.cpp SceneAudit.cpp)
target_link_libraries(territory_tests PRIVATE territory unreal geodata geometry rendering)
target_sources(territory_tests PRIVATE RasterTests.cpp)
target_sources(territory_tests PRIVATE GpuRasterTests.cpp)
target_sources(territory_tests PRIVATE OutputTests.cpp)
target_sources(territory_tests PRIVATE RunnerTests.cpp)
target_sources(territory_tests PRIVATE ProcessTests.cpp ${CMAKE_SOURCE_DIR}/application/src/TerritoryProcess.cpp ${CMAKE_SOURCE_DIR}/application/src/TerritoryRenderController.cpp)
target_include_directories(territory_tests PRIVATE ${CMAKE_SOURCE_DIR}/application/src)
target_sources(territory_tests PRIVATE CliTests.cpp UiTests.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryCommandLine.cpp
    ${CMAKE_SOURCE_DIR}/application/src/TerritoryRenderPolicy.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ClientSessionContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ClientStartup.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapSelectionContext.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapResidency.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapGridTransform.cpp
    ${CMAKE_SOURCE_DIR}/application/src/MapCatalog.cpp)
target_include_directories(territory_tests SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs/stb/stb)
target_include_directories(territory_tests PRIVATE ..)
set_target_properties(territory_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(territory_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
add_test(NAME territory_tests COMMAND territory_tests)
add_executable(territory_png_compare PngCompare.cpp)
target_link_libraries(territory_png_compare PRIVATE territory)
target_include_directories(territory_png_compare SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/libs/stb/stb)
set_target_properties(territory_png_compare PROPERTIES ${TARGET_PROPERTIES})
if(WIN32)
    target_link_libraries(territory_png_compare PRIVATE shell32)
endif()
add_test(NAME territory_cli_contract COMMAND ${CMAKE_COMMAND}
    "-DAPP=$<TARGET_FILE:l2mapconv>" -P "${CMAKE_CURRENT_SOURCE_DIR}/CliIntegrationTests.cmake")
if(WIN32)
    target_link_libraries(territory_tests PRIVATE shell32)
    add_executable(territory_fake_worker FakeWorker.cpp)
    target_link_libraries(territory_fake_worker PRIVATE territory shell32)
    set_target_properties(territory_fake_worker PROPERTIES ${TARGET_PROPERTIES})
    add_dependencies(territory_tests territory_fake_worker)
endif()
