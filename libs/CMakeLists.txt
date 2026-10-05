# GLEW
option(glew-cmake_BUILD_SHARED "Build shared glew library" OFF)
set(glew-cmake_BUILD_SHARED OFF)
option(glew-cmake_BUILD_MULTI_CONTEXT "Build the multi-context glew library" OFF)
set(glew-cmake_BUILD_MULTI_CONTEXT OFF)
add_subdirectory(glew)

# Recast
option(RECASTNAVIGATION_DEMO "Build demo" OFF)
set(RECASTNAVIGATION_DEMO OFF)
option(RECASTNAVIGATION_TESTS "Build tests" OFF)
set(RECASTNAVIGATION_TESTS OFF)
option(RECASTNAVIGATION_EXAMPLES "Build examples" OFF)
set(RECASTNAVIGATION_EXAMPLES OFF)
add_subdirectory(recast)
target_compile_definitions(Detour PUBLIC DT_POLYREF64)

# Apply Recast patch
find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} apply --reverse --check ${CMAKE_CURRENT_SOURCE_DIR}/patches/recast.patch
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/recast
    RESULT_VARIABLE RECAST_PATCH_PRESENT OUTPUT_QUIET ERROR_QUIET
)
if(NOT RECAST_PATCH_PRESENT EQUAL 0)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply --check ${CMAKE_CURRENT_SOURCE_DIR}/patches/recast.patch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/recast
        RESULT_VARIABLE RECAST_PATCH_CHECK ERROR_VARIABLE RECAST_PATCH_ERROR
    )
    if(NOT RECAST_PATCH_CHECK EQUAL 0)
        message(FATAL_ERROR "Recast patch conflicts with current sources: ${RECAST_PATCH_ERROR}")
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/recast.patch
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/recast
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

# GLFW
option(GLFW_BUILD_DOCS "Build docs" OFF)
set(GLFW_BUILD_DOCS OFF)
add_subdirectory(glfw)

# Other libs
add_subdirectory(llvm)
add_subdirectory(glm)
add_subdirectory(imgui)
add_subdirectory(stb)
add_subdirectory(cxxopts)
add_subdirectory(json)
