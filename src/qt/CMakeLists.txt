find_package(Qt6 REQUIRED COMPONENTS PrintSupport Widgets)

add_library(jwpqt_qt_support STATIC
  application_settings.cpp
  application_settings.h
  edict_resource_search.cpp
  edict_resource_search.h
  edict_resources.cpp
  edict_resources.h
  file_io.cpp
  file_io.h
  kanji_color_settings.cpp
  kanji_color_settings.h
  kanji_info_options.cpp
  kanji_info_options.h
  project_workspace.cpp
  project_workspace.h
  recent_files.cpp
  recent_files.h
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
  application_settings_dialog.cpp
  application_settings_dialog.h
  japanese_fonts.cpp
  japanese_fonts.h
  character_context_menu.cpp
  character_context_menu.h
  edict_lookup_dialog.cpp
  edict_lookup_dialog.h
  edict_results_window.cpp
  edict_results_window.h
  edict_user_dictionary_dialog.cpp
  edict_user_dictionary_dialog.h
  jis_table_dialog.cpp
  jis_table_dialog.h
  jwp_editor.cpp
  jwp_editor.h
  input_mode.h
  kana_input_field.cpp
  kana_input_field.h
  kanji_code_lookup_dialog.cpp
  kanji_code_lookup_dialog.h
  kanji_count_dialog.cpp
  kanji_count_dialog.h
  kanji_info_dialog.cpp
  kanji_info_dialog.h
  kanji_info_options_dialog.cpp
  kanji_info_options_dialog.h
  kanji_lookup_dialog.cpp
  kanji_lookup_dialog.h
  kanji_reading_lookup_dialog.cpp
  kanji_reading_lookup_dialog.h
  lookup_artwork.cpp
  lookup_artwork.h
  main_window.cpp
  main_window.h
  page_layout_dialog.cpp
  page_layout_dialog.h
  print_document.cpp
  print_document.h
  wnn_user_dictionary_dialog.cpp
  wnn_user_dictionary_dialog.h
)

qt_add_resources(jwpqt_qt_ui toolbar_assets
  PREFIX "/jwpqt"
  BASE "${PROJECT_SOURCE_DIR}"
  FILES "${PROJECT_SOURCE_DIR}/toolbar.bmp"
        "${PROJECT_SOURCE_DIR}/hsradicals.bmp"
        "${PROJECT_SOURCE_DIR}/skiptype.bmp"
        "${PROJECT_SOURCE_DIR}/fourcorners.bmp"
)

target_link_libraries(jwpqt_qt_ui
  PUBLIC
    jwpqt_qt_support
    Qt6::PrintSupport
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
