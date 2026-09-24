# Run on every build, including after a commit without source changes.
# file(CONFIGURE) only changes the header timestamp when the identity changes.
set(revision unknown)
execute_process(COMMAND "${GIT}" -C "${SOURCE_DIR}" rev-parse --short=12 HEAD
    RESULT_VARIABLE result OUTPUT_VARIABLE candidate OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if("${result}" STREQUAL "0" AND candidate MATCHES "^[0-9a-f]+$")
    set(revision "${candidate}")
    execute_process(COMMAND "${GIT}" -C "${SOURCE_DIR}" diff-index --quiet HEAD --
        RESULT_VARIABLE dirty ERROR_QUIET)
    if(NOT "${dirty}" STREQUAL "0")
        string(APPEND revision "-dirty")
    endif()
endif()
set(profile "geometry preview")
if(LOAD_TEXTURES)
    set(profile "legacy textured preview")
endif()
file(CONFIGURE OUTPUT "${OUTPUT}" CONTENT [=[
#pragma once
inline constexpr const char *build_identity =
    "l2mapconv P542 preview (git @revision@; @profile@)";
]=] @ONLY)
