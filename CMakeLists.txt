cmake_minimum_required(VERSION 3.10)

project(movie_stream C)

set(SOURCES
    src/main.c
    src/site.c
    src/list.c
)

add_executable(movie_stream ${SOURCES})

target_include_directories(movie_stream PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# Explicitly find and link pthread for multithreading
find_package(Threads REQUIRED)
target_link_libraries(movie_stream PRIVATE Threads::Threads)

target_compile_options(movie_stream PRIVATE 
    -Wall -Wextra -Wpedantic -Werror
)