find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(jwpqt_qt_support STATIC
  edict_resources.cpp
  edict_resources.h
  file_io.cpp
  file_io.h
  kanji_color_settings.cpp
  kanji_color_settings.h
  text_bridge.cpp
  text_bridge.h
)

target_include_directories(jwpqt_qt_support
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(jwpqt_qt_support
  PUBLIC
    jwpqt_core
    Qt6::Core
)

target_compile_options(jwpqt_qt_support
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_library(jwpqt_qt_ui STATIC
  jwp_editor.cpp
  jwp_editor.h
  main_window.cpp
  main_window.h
  wnn_user_dictionary_dialog.cpp
  wnn_user_dictionary_dialog.h
)

target_link_libraries(jwpqt_qt_ui
  PUBLIC
    jwpqt_qt_support
    Qt6::Widgets
)

target_compile_options(jwpqt_qt_ui
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

qt_add_executable(jwpqt
  main.cpp
)

target_link_libraries(jwpqt PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS jwpqt RUNTIME DESTINATION bin)
