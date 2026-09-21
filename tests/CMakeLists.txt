add_executable(l2mapconv_tests
    TestMain.cpp
)

set_target_properties(l2mapconv_tests PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(l2mapconv_tests PRIVATE ${TARGET_COMPILE_OPTIONS})

add_test(NAME l2mapconv_tests COMMAND l2mapconv_tests)
