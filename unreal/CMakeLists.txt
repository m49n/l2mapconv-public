cmake_minimum_required(VERSION 3.17)
project(unreal)

add_library(${PROJECT_NAME}
    src/pch.cpp
    src/Debug.cpp

    # Archive loading
    src/PackageLoader.cpp
    src/ArchiveLoader.cpp
    src/Decryptor.cpp
    src/Archive.cpp

    # Property extraction
    src/PropertyExtractor.cpp
    src/ArrayIndex.cpp
    src/Property.cpp

    # Object loading
    src/ObjectLoader.cpp
    src/Object.cpp

    # Objects
    src/Primitives.cpp
    src/Level.cpp
    src/BSP.cpp
    src/StaticMesh.cpp
    src/Material.cpp
    src/Actor.cpp
    src/Terrain.cpp
    src/MaterialDeserializer.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PUBLIC utils
)

# Compiler options
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

# Test executable
add_executable(${PROJECT_NAME}_test src/main.cpp)
target_link_libraries(${PROJECT_NAME}_test ${PROJECT_NAME})

set_target_properties(${PROJECT_NAME}_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_compile_options(${PROJECT_NAME}_test PRIVATE -Wall -Wextra -pedantic)
