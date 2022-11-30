cmake_minimum_required(VERSION 3.23)
project(Projekt)

set(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)

add_executable(Projekt main.cpp)

target_link_libraries(Projekt PUBLIC MPI::MPI_CXX)
