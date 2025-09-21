cmake_minimum_required(VERSION 3.10)

project(movie_stream)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil)

add_executable(movie_stream main.c site.c site.h list.c list.h)

target_include_directories(movie_stream PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(movie_stream ${FFMPEG_LIBRARIES})

target_compile_options(movie_stream PRIVATE 
    -Wall -Wextra -Wpedantic -Werror
)