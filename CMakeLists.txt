cmake_minimum_required(VERSION 3.20)
project(ia_block_blast LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV QUIET)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR
        "OpenCV introuvable.\n"
        "Installez le paquet devel : sudo dnf install opencv-devel")
endif()
find_package(Threads REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(ia_block_blast ${SOURCES})

target_include_directories(ia_block_blast PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ia_block_blast PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
)

target_compile_options(ia_block_blast PRIVATE -O2 -Wall -Wextra -Wpedantic)
