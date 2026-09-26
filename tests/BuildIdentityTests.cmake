execute_process(COMMAND "${APP}" --version
    RESULT_VARIABLE code OUTPUT_VARIABLE version ERROR_VARIABLE diagnostic TIMEOUT 10)
if(NOT "${code}" STREQUAL "0" OR
   NOT version MATCHES "^l2mapconv P542 preview \\(git ([0-9a-f]+(-dirty)?|unknown); (live material preview|live \\+ legacy textured preview)\\)")
    message(FATAL_ERROR "Build identity unavailable without a client: ${code}: ${version} ${diagnostic}")
endif()
execute_process(COMMAND "${APP}" --help
    RESULT_VARIABLE code OUTPUT_VARIABLE help ERROR_VARIABLE diagnostic TIMEOUT 10)
foreach(option --version --preview --build --render-territory --inspect-territory)
    if(NOT "${code}" STREQUAL "0" OR NOT help MATCHES "${option}")
        message(FATAL_ERROR "Unified executable does not advertise ${option}: ${help} ${diagnostic}")
    endif()
endforeach()
