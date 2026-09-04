add_executable(jwpqt_core_tests
  utf8_test.cpp
)

target_link_libraries(jwpqt_core_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_core_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_qt_io_tests
  qt_file_io_test.cpp
)
target_link_libraries(jwpqt_qt_io_tests PRIVATE jwpqt_qt_support)
target_compile_options(jwpqt_qt_io_tests PRIVATE -Wall -Wextra -Wpedantic)

add_executable(jwpqt_main_window_tests
  main_window_test.cpp
)
target_link_libraries(jwpqt_main_window_tests PRIVATE jwpqt_qt_ui)
target_compile_options(jwpqt_main_window_tests PRIVATE -Wall -Wextra -Wpedantic)

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

add_executable(jwpqt_legacy_text_tests
  legacy_text_test.cpp
)
target_link_libraries(jwpqt_legacy_text_tests PRIVATE jwpqt_core)
target_compile_options(jwpqt_legacy_text_tests PRIVATE -Wall -Wextra -Wpedantic)

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
add_test(NAME core_jis_encoding COMMAND jwpqt_jis_encoding_tests)
add_test(NAME core_jis_unicode COMMAND jwpqt_jis_unicode_tests)
add_test(NAME core_jis_text COMMAND jwpqt_jis_text_tests)
add_test(NAME core_legacy_text COMMAND jwpqt_legacy_text_tests)
add_test(NAME core_text_file COMMAND jwpqt_text_file_tests)
add_test(NAME core_text_detection COMMAND jwpqt_text_detection_tests)
add_test(NAME qt_file_io COMMAND jwpqt_qt_io_tests)
add_test(NAME qt_main_window COMMAND jwpqt_main_window_tests)
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
add_test(NAME application_invalid_encoding
  COMMAND "${CMAKE_COMMAND}"
          "-DJWPQT_EXECUTABLE=$<TARGET_FILE:jwpqt>"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/check_invalid_encoding.cmake"
)
set_tests_properties(
  application_startup
  application_open_utf8
  application_encoding_euc_jp
  application_encoding_shift_jis
  application_encoding_new_jis
  application_encoding_old_jis
  application_encoding_nec_jis
  application_invalid_encoding
  PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
set_tests_properties(qt_main_window PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
