add_library(jwpqt_core STATIC
  byte_io.cpp
  jis_encoding.cpp
  jis_text.cpp
  jis_unicode.cpp
  jwp_document.cpp
  legacy_text.cpp
  text_detection.cpp
  text_file.cpp
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
