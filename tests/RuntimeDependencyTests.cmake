# Check actual PE imports, including transitive CRT dependencies of packaged DLLs.
# Running the EXE alone can silently use an already installed system VC runtime.
file(GLOB binaries "${stage}/*.exe" "${stage}/*.dll")
foreach(binary IN LISTS binaries)
    execute_process(COMMAND "${OBJDUMP}" -p "${binary}"
        RESULT_VARIABLE code OUTPUT_VARIABLE imports ERROR_VARIABLE diagnostic TIMEOUT 10)
    if(NOT "${code}" STREQUAL "0" OR NOT imports MATCHES "DLL Name:")
        message(FATAL_ERROR "Cannot inspect PE imports of ${binary}: ${diagnostic}")
    endif()
    string(TOLOWER "${imports}" imports)
    string(REGEX MATCHALL "dll name: [^\r\n ]+" dependencies "${imports}")
    foreach(dependency IN LISTS dependencies)
        string(REPLACE "dll name: " "" name "${dependency}")
        if(name MATCHES "^(msvcp|msvcr|vcruntime|concrt)[0-9].*\\.dll$" AND
           NOT EXISTS "${stage}/${name}")
            message(FATAL_ERROR "Runtime package is missing imported VC library ${name} (${binary})")
        endif()
    endforeach()
endforeach()
