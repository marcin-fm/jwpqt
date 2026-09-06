add_executable(jwpqt_core_tests
  utf8_test.cpp
)

target_link_libraries(jwpqt_core_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_core_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_byte_io_tests
  byte_io_test.cpp
)
target_link_libraries(jwpqt_byte_io_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_byte_io_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_edict_dictionary_tests
  edict_dictionary_test.cpp
)
target_link_libraries(jwpqt_edict_dictionary_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_dictionary_tests
  PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_edict_user_dictionary_tests
  edict_user_dictionary_test.cpp
)
target_link_libraries(jwpqt_edict_user_dictionary_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_user_dictionary_tests
  PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_qt_io_tests
  qt_file_io_test.cpp
)
target_link_libraries(jwpqt_qt_io_tests PRIVATE jwpqt_qt_support)
target_compile_options(jwpqt_qt_io_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_edict_resource_search_tests
  edict_resource_search_test.cpp
)
target_link_libraries(jwpqt_edict_resource_search_tests
  PRIVATE jwpqt_qt_support)
target_compile_options(jwpqt_edict_resource_search_tests
  PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_kanji_color_settings_tests
  kanji_color_settings_test.cpp
)
target_link_libraries(jwpqt_kanji_color_settings_tests
  PRIVATE jwpqt_qt_support)
target_compile_options(jwpqt_kanji_color_settings_tests
  PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_main_window_tests
  main_window_test.cpp
)
target_link_libraries(jwpqt_main_window_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_main_window_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_wnn_user_dictionary_dialog_tests
  wnn_user_dictionary_dialog_test.cpp
)
target_link_libraries(jwpqt_wnn_user_dictionary_dialog_tests
  PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_wnn_user_dictionary_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jis_encoding_tests
  jis_encoding_test.cpp
)
target_link_libraries(jwpqt_jis_encoding_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jis_encoding_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jis_unicode_tests
  jis_unicode_test.cpp
)
target_link_libraries(jwpqt_jis_unicode_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jis_unicode_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jis_text_tests
  jis_text_test.cpp
)
target_link_libraries(jwpqt_jis_text_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jis_text_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_kana_input_tests
  kana_input_test.cpp
)
target_link_libraries(jwpqt_kana_input_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kana_input_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_wnn_dictionary_tests
  wnn_dictionary_test.cpp
)
target_link_libraries(jwpqt_wnn_dictionary_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_wnn_dictionary_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_wnn_lookup_tests
  wnn_lookup_test.cpp
)
target_link_libraries(jwpqt_wnn_lookup_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_wnn_lookup_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_wnn_preferences_tests
  wnn_preferences_test.cpp
)
target_link_libraries(jwpqt_wnn_preferences_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_wnn_preferences_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_document_tests
  jwp_document_test.cpp
)
target_link_libraries(jwpqt_jwp_document_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_document_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_text_codec_tests
  jwp_text_codec_test.cpp
)
target_link_libraries(jwpqt_jwp_text_codec_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_text_codec_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_document_model_tests
  jwp_document_model_test.cpp
)
target_link_libraries(jwpqt_jwp_document_model_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_document_model_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_document_history_tests
  jwp_document_history_test.cpp
)
target_link_libraries(jwpqt_jwp_document_history_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_document_history_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_plain_text_tests
  jwp_plain_text_test.cpp
)
target_link_libraries(jwpqt_jwp_plain_text_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_plain_text_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_jwp_search_tests
  jwp_search_test.cpp
)
target_link_libraries(jwpqt_jwp_search_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_search_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_plain_text_change_tests
  plain_text_change_test.cpp
)
target_link_libraries(jwpqt_plain_text_change_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_plain_text_change_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_legacy_text_tests
  legacy_text_test.cpp
)
target_link_libraries(jwpqt_legacy_text_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_legacy_text_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_legacy_code_page_tests
  legacy_code_page_test.cpp
)
target_link_libraries(jwpqt_legacy_code_page_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_legacy_code_page_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_text_file_tests
  text_file_test.cpp
)
target_link_libraries(jwpqt_text_file_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_text_file_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_text_detection_tests
  text_detection_test.cpp
)
target_link_libraries(jwpqt_text_detection_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_text_detection_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME core_utf8 COMMAND jwpqt_core_tests)

add_executable(jwpqt_utf7_tests
  utf7_test.cpp
)
target_link_libraries(jwpqt_utf7_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_utf7_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_utf7 COMMAND jwpqt_utf7_tests)

add_executable(jwpqt_jfc_text_tests
  jfc_text_test.cpp
)
target_link_libraries(jwpqt_jfc_text_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jfc_text_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_jfc_text COMMAND jwpqt_jfc_text_tests)

add_test(NAME core_byte_io COMMAND jwpqt_byte_io_tests)
add_test(NAME core_edict_dictionary COMMAND jwpqt_edict_dictionary_tests)
add_test(NAME core_edict_user_dictionary
  COMMAND jwpqt_edict_user_dictionary_tests)

add_executable(jwpqt_edict_filter_tests
  edict_filter_test.cpp
)
target_link_libraries(jwpqt_edict_filter_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_filter_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_filter COMMAND jwpqt_edict_filter_tests)

add_executable(jwpqt_edict_engine_tests
  edict_engine_test.cpp
)
target_link_libraries(jwpqt_edict_engine_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_engine_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_engine COMMAND jwpqt_edict_engine_tests)

add_executable(jwpqt_edict_deinflection_tests
  edict_deinflection_test.cpp
)
target_link_libraries(jwpqt_edict_deinflection_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_deinflection_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_deinflection
  COMMAND jwpqt_edict_deinflection_tests)
add_test(NAME core_jis_encoding COMMAND jwpqt_jis_encoding_tests)

add_executable(jwpqt_jis_table_tests
  jis_table_test.cpp
)
target_link_libraries(jwpqt_jis_table_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jis_table_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_jis_table COMMAND jwpqt_jis_table_tests)
add_executable(jwpqt_kanji_bushu_selector_tests
  kanji_bushu_selector_test.cpp
)
target_link_libraries(jwpqt_kanji_bushu_selector_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_bushu_selector_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_bushu_selector
  COMMAND jwpqt_kanji_bushu_selector_tests)
add_test(NAME core_jis_unicode COMMAND jwpqt_jis_unicode_tests)
add_test(NAME core_jis_text COMMAND jwpqt_jis_text_tests)
add_executable(jwpqt_kanji_color_tests
  kanji_color_test.cpp
)
target_link_libraries(jwpqt_kanji_color_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_color_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_color COMMAND jwpqt_kanji_color_tests)

add_executable(jwpqt_kanji_info_tests
  kanji_info_test.cpp
)
target_link_libraries(jwpqt_kanji_info_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_info_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_info COMMAND jwpqt_kanji_info_tests)

add_executable(jwpqt_kanji_lookup_lists_tests
  kanji_lookup_lists_test.cpp
)
target_link_libraries(jwpqt_kanji_lookup_lists_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_lookup_lists_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_lookup_lists COMMAND jwpqt_kanji_lookup_lists_tests)

add_executable(jwpqt_kanji_lookup_tests
  kanji_lookup_test.cpp
)
target_link_libraries(jwpqt_kanji_lookup_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_lookup_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_lookup COMMAND jwpqt_kanji_lookup_tests)

add_executable(jwpqt_kanji_code_search_tests
  kanji_code_search_test.cpp
)
target_link_libraries(jwpqt_kanji_code_search_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_code_search_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_code_search COMMAND jwpqt_kanji_code_search_tests)

add_executable(jwpqt_kanji_reading_search_tests
  kanji_reading_search_test.cpp
)
target_link_libraries(jwpqt_kanji_reading_search_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_reading_search_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_reading_search
  COMMAND jwpqt_kanji_reading_search_tests)
add_executable(jwpqt_kanji_color_list_tests
  kanji_color_list_test.cpp
)
target_link_libraries(jwpqt_kanji_color_list_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_color_list_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_color_list COMMAND jwpqt_kanji_color_list_tests)

add_executable(jwpqt_kanji_count_tests
  kanji_count_test.cpp
)
target_link_libraries(jwpqt_kanji_count_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_kanji_count_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_kanji_count COMMAND jwpqt_kanji_count_tests)
add_test(NAME core_kana_input COMMAND jwpqt_kana_input_tests)
add_test(NAME core_wnn_dictionary COMMAND jwpqt_wnn_dictionary_tests)
add_test(NAME core_wnn_lookup COMMAND jwpqt_wnn_lookup_tests)
add_test(NAME core_wnn_preferences COMMAND jwpqt_wnn_preferences_tests)

add_executable(jwpqt_wnn_user_dictionary_tests
  wnn_user_dictionary_test.cpp
)
target_link_libraries(jwpqt_wnn_user_dictionary_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_wnn_user_dictionary_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_wnn_user_dictionary COMMAND jwpqt_wnn_user_dictionary_tests)

add_executable(jwpqt_edict_index_tests
  edict_index_test.cpp
)
target_link_libraries(jwpqt_edict_index_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_index_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_index COMMAND jwpqt_edict_index_tests)

add_executable(jwpqt_edict_pattern_tests
  edict_pattern_test.cpp
)
target_link_libraries(jwpqt_edict_pattern_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_pattern_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_pattern COMMAND jwpqt_edict_pattern_tests)

add_executable(jwpqt_edict_pattern_match_tests
  edict_pattern_match_test.cpp
)
target_link_libraries(jwpqt_edict_pattern_match_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_pattern_match_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_pattern_match COMMAND jwpqt_edict_pattern_match_tests)

add_executable(jwpqt_edict_search_tests
  edict_search_test.cpp
)
target_link_libraries(jwpqt_edict_search_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_search_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_search COMMAND jwpqt_edict_search_tests)

add_executable(jwpqt_edict_registry_tests
  edict_registry_test.cpp
)
target_link_libraries(jwpqt_edict_registry_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_edict_registry_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_edict_registry COMMAND jwpqt_edict_registry_tests)

add_executable(jwpqt_wnn_session_tests
  wnn_session_test.cpp
)

target_link_libraries(jwpqt_wnn_session_tests
  PRIVATE
    jwpqt_core
)

target_compile_options(jwpqt_wnn_session_tests
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

add_test(NAME core_wnn_session COMMAND jwpqt_wnn_session_tests)

add_executable(jwpqt_jwp_conversion_tests
  jwp_conversion_test.cpp
)

target_link_libraries(jwpqt_jwp_conversion_tests
  PRIVATE
    jwpqt_core
)

target_compile_options(jwpqt_jwp_conversion_tests
  PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
)

add_test(NAME core_jwp_conversion COMMAND jwpqt_jwp_conversion_tests)
add_test(NAME core_jwp_document COMMAND jwpqt_jwp_document_tests)
add_test(NAME core_jwp_text_codec COMMAND jwpqt_jwp_text_codec_tests)
add_test(NAME core_jwp_document_model COMMAND jwpqt_jwp_document_model_tests)
add_test(NAME core_jwp_document_history COMMAND jwpqt_jwp_document_history_tests)
add_test(NAME core_jwp_plain_text COMMAND jwpqt_jwp_plain_text_tests)
add_executable(jwpqt_jwp_project_tests
  jwp_project_test.cpp
)
target_link_libraries(jwpqt_jwp_project_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_jwp_project_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME core_jwp_project COMMAND jwpqt_jwp_project_tests)
add_test(NAME core_jwp_search COMMAND jwpqt_jwp_search_tests)
add_test(NAME core_plain_text_change COMMAND jwpqt_plain_text_change_tests)
add_test(NAME core_legacy_code_page COMMAND jwpqt_legacy_code_page_tests)
add_test(NAME core_legacy_text COMMAND jwpqt_legacy_text_tests)
add_test(NAME core_text_file COMMAND jwpqt_text_file_tests)
add_test(NAME core_text_detection COMMAND jwpqt_text_detection_tests)
add_test(NAME qt_file_io COMMAND jwpqt_qt_io_tests)
add_executable(jwpqt_edict_resources_tests
  edict_resources_test.cpp
)
target_link_libraries(jwpqt_edict_resources_tests PRIVATE jwpqt_qt_support)
target_compile_options(jwpqt_edict_resources_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_edict_resources COMMAND jwpqt_edict_resources_tests)
add_test(NAME qt_edict_resource_search COMMAND jwpqt_edict_resource_search_tests)

add_executable(jwpqt_edict_lookup_dialog_tests
  edict_lookup_dialog_test.cpp
)
target_link_libraries(jwpqt_edict_lookup_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_edict_lookup_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_edict_lookup_dialog COMMAND jwpqt_edict_lookup_dialog_tests)

add_executable(jwpqt_edict_results_window_tests
  edict_results_window_test.cpp
)
target_link_libraries(jwpqt_edict_results_window_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_edict_results_window_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_edict_results_window COMMAND jwpqt_edict_results_window_tests)

add_executable(jwpqt_edict_user_dictionary_dialog_tests
  edict_user_dictionary_dialog_test.cpp
)
target_link_libraries(jwpqt_edict_user_dictionary_dialog_tests
  PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_edict_user_dictionary_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_edict_user_dictionary_dialog
  COMMAND jwpqt_edict_user_dictionary_dialog_tests)

add_executable(jwpqt_jis_table_dialog_tests
  jis_table_dialog_test.cpp
)
target_link_libraries(jwpqt_jis_table_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_jis_table_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_jis_table_dialog COMMAND jwpqt_jis_table_dialog_tests)
set_tests_properties(qt_jis_table_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_test(NAME qt_kanji_color_settings COMMAND jwpqt_kanji_color_settings_tests)
add_test(NAME qt_main_window COMMAND jwpqt_main_window_tests)
add_executable(jwpqt_runtime_environment_tests runtime_environment_test.cpp)
target_link_libraries(jwpqt_runtime_environment_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_runtime_environment_tests PRIVATE -Wall -Wextra -Wpedantic)
add_dependencies(jwpqt_runtime_environment_tests jwpqt)
add_test(NAME qt_runtime_environment
  COMMAND jwpqt_runtime_environment_tests $<TARGET_FILE:jwpqt>)
set_tests_properties(qt_runtime_environment PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg-config"
  TIMEOUT 30)
add_test(NAME qt_wnn_user_dictionary_dialog
  COMMAND jwpqt_wnn_user_dictionary_dialog_tests)
add_executable(jwpqt_jwp_editor_tests
  jwp_editor_test.cpp
)
target_link_libraries(jwpqt_jwp_editor_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_jwp_editor_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_jwp_editor COMMAND jwpqt_jwp_editor_tests)
add_executable(jwpqt_kanji_info_dialog_tests
  kanji_info_dialog_test.cpp
)
target_link_libraries(jwpqt_kanji_info_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_info_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_info_dialog COMMAND jwpqt_kanji_info_dialog_tests)
add_executable(jwpqt_kanji_info_integration_tests
  kanji_info_integration_test.cpp
)
target_link_libraries(jwpqt_kanji_info_integration_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_info_integration_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_info_integration COMMAND jwpqt_kanji_info_integration_tests)
add_executable(jwpqt_kanji_lookup_dialog_tests
  kanji_lookup_dialog_test.cpp
)
target_link_libraries(jwpqt_kanji_lookup_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_lookup_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_lookup_dialog COMMAND jwpqt_kanji_lookup_dialog_tests)
add_executable(jwpqt_kanji_code_lookup_dialog_tests
  kanji_code_lookup_dialog_test.cpp
)
target_link_libraries(jwpqt_kanji_code_lookup_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_code_lookup_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_code_lookup_dialog COMMAND jwpqt_kanji_code_lookup_dialog_tests)
add_executable(jwpqt_kanji_count_dialog_tests
  kanji_count_dialog_test.cpp
)
target_link_libraries(jwpqt_kanji_count_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_count_dialog_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_count_dialog COMMAND jwpqt_kanji_count_dialog_tests)

add_executable(jwpqt_page_layout_dialog_tests
  page_layout_dialog_test.cpp
)
target_link_libraries(jwpqt_page_layout_dialog_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_page_layout_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_page_layout_dialog COMMAND jwpqt_page_layout_dialog_tests)
set_tests_properties(qt_page_layout_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(jwpqt_print_document_tests
  print_document_test.cpp
)
target_link_libraries(jwpqt_print_document_tests
  PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_print_document_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_print_document COMMAND jwpqt_print_document_tests)
set_tests_properties(qt_print_document PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
add_executable(jwpqt_kanji_reading_lookup_dialog_tests
  kanji_reading_lookup_dialog_test.cpp
)
target_link_libraries(jwpqt_kanji_reading_lookup_dialog_tests
  PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_kanji_reading_lookup_dialog_tests
  PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME qt_kanji_reading_lookup_dialog
  COMMAND jwpqt_kanji_reading_lookup_dialog_tests)
add_test(NAME application_startup COMMAND jwpqt --smoke-test)
add_test(NAME application_open_utf8
  COMMAND jwpqt --smoke-test
          "${CMAKE_CURRENT_SOURCE_DIR}/data/sample-utf8.txt"
)
string(ASCII 65 83 67 73 73 32 164 162 198 252 203 220 184 236 10
  sample_euc_jp)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-euc-jp.txt" "${sample_euc_jp}")
string(ASCII 65 83 67 73 73 32 130 160 147 250 150 123 140 234 10
  sample_shift_jis)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-shift-jis.txt"
  "${sample_shift_jis}")
string(ASCII 65 83 67 73 73 32 27 36 66 36 34 70 124 75 92 56 108 27 40 74 10
  sample_new_jis)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-new-jis.txt"
  "${sample_new_jis}")
string(ASCII 65 83 67 73 73 32 27 36 64 36 34 70 124 75 92 56 108 27 40 74 10
  sample_old_jis)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-old-jis.txt"
  "${sample_old_jis}")
string(ASCII 65 83 67 73 73 32 27 75 36 34 70 124 75 92 56 108 27 72 10
  sample_nec_jis)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-nec-jis.txt"
  "${sample_nec_jis}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-utf7.txt"
  "ASCII +ZeVnLIqe-\n")
string(ASCII 198 252 9 142 38 143 171 177 10 sample_jfc_euc)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-jfc-euc.txt" "${sample_jfc_euc}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-euc.jfc" "${sample_jfc_euc}")
string(ASCII 195 169 9 240 159 152 128 10 sample_jfc_utf8)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-utf8.jfc" "${sample_jfc_utf8}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-ascii.JFC" "question\tanswer\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sample-ascii.txt" "ASCII only\n")
add_test(NAME application_encoding_euc_jp
  COMMAND jwpqt --smoke-test --encoding euc-jp
          "${CMAKE_CURRENT_BINARY_DIR}/sample-euc-jp.txt"
)
add_test(NAME application_encoding_shift_jis
  COMMAND jwpqt --smoke-test --encoding shift-jis
          "${CMAKE_CURRENT_BINARY_DIR}/sample-shift-jis.txt"
)
add_test(NAME application_encoding_new_jis
  COMMAND jwpqt --smoke-test --encoding new-jis
          "${CMAKE_CURRENT_BINARY_DIR}/sample-new-jis.txt"
)
add_test(NAME application_encoding_old_jis
  COMMAND jwpqt --smoke-test --encoding old-jis
          "${CMAKE_CURRENT_BINARY_DIR}/sample-old-jis.txt"
)
add_test(NAME application_encoding_nec_jis
  COMMAND jwpqt --smoke-test --encoding nec-jis
          "${CMAKE_CURRENT_BINARY_DIR}/sample-nec-jis.txt"
)
add_test(NAME application_encoding_utf7
  COMMAND jwpqt --smoke-test --encoding utf-7
          "${CMAKE_CURRENT_BINARY_DIR}/sample-utf7.txt"
)
add_test(NAME application_encoding_jfc
  COMMAND jwpqt --smoke-test --encoding jfc
          "${CMAKE_CURRENT_BINARY_DIR}/sample-jfc-euc.txt"
)
add_test(NAME application_detect_jfc_euc
  COMMAND jwpqt --smoke-test "${CMAKE_CURRENT_BINARY_DIR}/sample-euc.jfc"
)
add_test(NAME application_detect_jfc_utf8
  COMMAND jwpqt --smoke-test "${CMAKE_CURRENT_BINARY_DIR}/sample-utf8.jfc"
)
add_test(NAME application_detect_jfc_ascii
  COMMAND jwpqt --smoke-test "${CMAKE_CURRENT_BINARY_DIR}/sample-ascii.JFC"
)
add_test(NAME application_detect_euc_jp
  COMMAND jwpqt --smoke-test
          "${CMAKE_CURRENT_BINARY_DIR}/sample-euc-jp.txt"
)
add_test(NAME application_detect_shift_jis
  COMMAND jwpqt --smoke-test
          "${CMAKE_CURRENT_BINARY_DIR}/sample-shift-jis.txt"
)
add_test(NAME application_detect_new_jis
  COMMAND jwpqt --smoke-test
          "${CMAKE_CURRENT_BINARY_DIR}/sample-new-jis.txt"
)
add_test(NAME application_invalid_encoding
  COMMAND "${CMAKE_COMMAND}"
          "-DJWPQT_EXECUTABLE=$<TARGET_FILE:jwpqt>"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_encoding.cmake"
)
add_test(NAME application_noninteractive_detection
  COMMAND "${CMAKE_COMMAND}"
          "-DJWPQT_EXECUTABLE=$<TARGET_FILE:jwpqt>"
          "-DASCII_FILE=${CMAKE_CURRENT_BINARY_DIR}/sample-ascii.txt"
          "-DEUC_FILE=${CMAKE_CURRENT_BINARY_DIR}/sample-euc-jp.txt"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/check_noninteractive_detection.cmake"
)
add_test(NAME application_invalid_kanji_configuration
  COMMAND "${CMAKE_COMMAND}"
          "-DJWPQT_EXECUTABLE=$<TARGET_FILE:jwpqt>"
          "-DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_kanji_configuration.cmake"
)
set_tests_properties(
  application_startup
  application_open_utf8
  application_encoding_euc_jp
  application_encoding_shift_jis
  application_encoding_new_jis
  application_encoding_old_jis
  application_encoding_nec_jis
  application_encoding_utf7
  application_encoding_jfc
  application_detect_jfc_euc
  application_detect_jfc_utf8
  application_detect_jfc_ascii
  application_detect_euc_jp
  application_detect_shift_jis
  application_detect_new_jis
  application_invalid_encoding
  application_noninteractive_detection
  application_invalid_kanji_configuration
  PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg-config"
  TIMEOUT 10
)
set_tests_properties(qt_main_window PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg-config"
  TIMEOUT 10
)
set_tests_properties(
  qt_edict_lookup_dialog
  qt_edict_results_window
  qt_edict_user_dictionary_dialog
  PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_wnn_user_dictionary_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_jwp_editor PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_info_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_info_integration PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_lookup_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_code_lookup_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_count_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_kanji_reading_lookup_dialog PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
