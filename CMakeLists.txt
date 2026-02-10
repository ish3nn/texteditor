cmake_minimum_required(VERSION 3.16)
project(texteditor C)

set(CMAKE_C_STANDARD 99)

find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)

add_executable(
        texteditor
        editor.c
        config.h
)

target_link_libraries(texteditor
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
)
