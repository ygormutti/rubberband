cmake_minimum_required(VERSION 2.8)

project(rubberband)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/core)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/core)

add_subdirectory(core)
link_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/core/lib)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
add_subdirectory(rbbs)

