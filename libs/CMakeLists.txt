option(glew-cmake_BUILD_SHARED "Build shared glew library" ON)
set(glew-cmake_BUILD_SHARED OFF)
option(glew-cmake_BUILD_MULTI_CONTEXT "Build the multi-context glew library" ON)
set(glew-cmake_BUILD_MULTI_CONTEXT OFF)

add_subdirectory(glew)
add_subdirectory(glfw)
add_subdirectory(glm)
add_subdirectory(imgui)
add_subdirectory(stb)

add_subdirectory(llvm)
