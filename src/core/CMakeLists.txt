add_library(jwpqt_core STATIC
  jis_encoding.cpp
  jis_unicode.cpp
  legacy_text.cpp
  utf8.cpp
)

target_include_directories(jwpqt_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_compile_options(jwpqt_core
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)
