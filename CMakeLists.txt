cmake_minimum_required(VERSION 3.31)
project(winmux)

set(CMAKE_CXX_STANDARD 20)

add_executable(winmux source/main.cpp)
if(MINGW)
  target_link_options(winmux PRIVATE -static)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(winmux PRIVATE -s)
  endif()
endif()