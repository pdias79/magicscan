cmake_minimum_required(VERSION 3.18)
project(magicscan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(magicscan
    src/main.cpp
    src/usb/probe.cpp
)
target_include_directories(magicscan PRIVATE src)
if(WIN32)
    target_compile_definitions(magicscan PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(magicscan PRIVATE setupapi)
endif()
if(MSVC)
    target_compile_options(magicscan PRIVATE /W4 /permissive-)
else()
    target_compile_options(magicscan PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME help COMMAND magicscan --help)
