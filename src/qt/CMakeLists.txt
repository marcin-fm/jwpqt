find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(jwpqt_qt_support STATIC
  edict_resource_search.cpp
  edict_resource_search.h
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
  edict_lookup_dialog.cpp
  edict_lookup_dialog.h
  edict_results_window.cpp
  edict_results_window.h
  edict_user_dictionary_dialog.cpp
  edict_user_dictionary_dialog.h
  jwp_editor.cpp
  jwp_editor.h
  kanji_code_lookup_dialog.cpp
  kanji_code_lookup_dialog.h
  kanji_count_dialog.cpp
  kanji_count_dialog.h
  kanji_info_dialog.cpp
  kanji_info_dialog.h
  kanji_lookup_dialog.cpp
  kanji_lookup_dialog.h
  kanji_reading_lookup_dialog.cpp
  kanji_reading_lookup_dialog.h
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
