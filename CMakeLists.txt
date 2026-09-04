cmake_minimum_required(VERSION 3.21)

project(jwpqt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_subdirectory(src/core)
add_subdirectory(src/qt)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
