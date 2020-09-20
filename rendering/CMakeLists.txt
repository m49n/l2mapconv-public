cmake_minimum_required(VERSION 3.17)
project(rendering)

add_library(${PROJECT_NAME}
    src/pch.cpp

    src/VertexBuffer.cpp
    src/Mesh.cpp
    src/Shader.cpp
    src/Camera.cpp
    src/Texture.cpp

    src/EntityMesh.cpp
    src/EntityShader.cpp
    src/Entity.cpp
    src/EntityTree.cpp
    src/Renderer.cpp

    src/ShaderLoader.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PRIVATE utils
    PRIVATE math

    # Third-party libs
    PRIVATE libglew_static
    PRIVATE glm
)

# Compiler options
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
