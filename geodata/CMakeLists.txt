cmake_minimum_required(VERSION 3.22)
project(geodata)

add_library(${PROJECT_NAME}
    src/pch.cpp
    src/L2JSerializer.cpp
    src/PtsSerializer.cpp
    src/Loader.cpp
    src/Exporter.cpp
    src/Map.cpp
    src/Builder.cpp
    src/NSWE.cpp
    src/ExportBuffer.cpp
    src/Compressor.cpp
)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/pch.h)

target_link_libraries(${PROJECT_NAME}
    PRIVATE utils
    PRIVATE geometry
    PRIVATE llvm

    PRIVATE glm
    PRIVATE Recast
)

# Compiler settings
set_target_properties(${PROJECT_NAME} PROPERTIES ${TARGET_PROPERTIES})
target_compile_options(${PROJECT_NAME} PRIVATE ${TARGET_COMPILE_OPTIONS})

# CMake options
option(L2MAPCONV_GEODATA_POST_PROCESSING "Geodata Post-Processing" ON)
if(L2MAPCONV_GEODATA_POST_PROCESSING)
  add_definitions(-DGEODATA_POST_PROCESSING)
endif()
