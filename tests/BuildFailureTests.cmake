# A regular file named output makes Exporter fail before rasterization.
file(MAKE_DIRECTORY "${FIXTURE}")
if(IS_DIRECTORY "${FIXTURE}/output")
    message(FATAL_ERROR "Build-failure fixture unexpectedly contains an output directory")
endif()
if(NOT EXISTS "${FIXTURE}/output")
    file(WRITE "${FIXTURE}/output" "build failure fixture")
endif()
file(SHA256 "${FIXTURE}/output" before)
execute_process(
    COMMAND "${APP}" --build --log-level 0 --client-root "${CLIENT}" -- 24_18
    WORKING_DIRECTORY "${FIXTURE}"
    RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr
    TIMEOUT 45)
if(NOT "${result}" STREQUAL "1" OR NOT stderr MATCHES "Geodata build failed:.*output")
    message(FATAL_ERROR "Expected exit 1 and a readable build failure, got ${result}: ${stderr}")
endif()
file(SHA256 "${FIXTURE}/output" after)
if(NOT before STREQUAL after)
    message(FATAL_ERROR "Failed build changed the conflicting output file")
endif()
