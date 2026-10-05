execute_process(COMMAND "${APP}" --geodata-job "${CMAKE_CURRENT_BINARY_DIR}/missing-geodata-job"
    RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT code EQUAL 2 OR NOT diagnostic MATCHES "Invalid geodata job")
    message(FATAL_ERROR "Geodata worker must reject a missing request without opening the viewer: ${code}: ${output} ${diagnostic}")
endif()
