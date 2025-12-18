cmake_minimum_required(VERSION 3.28)

if(MSVC)
    add_compile_options(/utf-8)
    add_definitions(-D_UNICODE -DUNICODE -D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)
endif()

project(FindTheBug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/shared)
add_subdirectory(src/infra)
add_subdirectory(src/storage)
add_subdirectory(src/engine)
add_subdirectory(src/server)