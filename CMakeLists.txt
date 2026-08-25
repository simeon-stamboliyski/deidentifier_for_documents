cmake_minimum_required(VERSION 3.20)

project(deidentifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(deidentifier
    main.cpp
    deidentifier/deidentifier.cpp
    detectors/detectors.cpp
    file_processor/file_processor.cpp
    metadata/metadata.cpp
)

target_include_directories(deidentifier PRIVATE
    deidentifier
    detectors
    file_processor
    metadata
)

if (MSVC)
    target_compile_options(deidentifier PRIVATE /W4)
else()
    target_compile_options(deidentifier PRIVATE -Wall -Wextra -Wpedantic)
endif()