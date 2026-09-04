add_library(jwpqt_core STATIC
  utf8.cpp
)

target_include_directories(jwpqt_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(jwpqt_core
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)
