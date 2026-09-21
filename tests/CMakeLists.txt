add_executable(l2mapconv_tests
    TestMain.cpp
    ${CMAKE_SOURCE_DIR}/application/src/CameraMotion.cpp
    ${CMAKE_SOURCE_DIR}/application/src/ExecutablePath.cpp
)

set_target_properties(l2mapconv_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(l2mapconv_tests PRIVATE ${TARGET_COMPILE_OPTIONS})
target_include_directories(l2mapconv_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/application/src
)
target_link_libraries(l2mapconv_tests PRIVATE geodata geometry unreal glm)

add_test(NAME l2mapconv_tests COMMAND l2mapconv_tests)
