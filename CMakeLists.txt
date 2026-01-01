cmake_minimum_required(VERSION 3.10)

project(movie_stream C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)

set(SOURCES
    src/main.c
    src/site.c
    src/list.c
    src/ffmpeg_utils.c
)

add_executable(movie_stream ${SOURCES})

target_include_directories(movie_stream PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(movie_stream PRIVATE ${FFMPEG_LIBRARIES})

target_compile_options(movie_stream PRIVATE 
    -Wall -Wextra -Wpedantic -Werror
)