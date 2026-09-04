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

add_test(NAME core_utf8 COMMAND jwpqt_core_tests)
add_test(NAME qt_file_io COMMAND jwpqt_qt_io_tests)
add_test(NAME application_startup COMMAND jwpqt --smoke-test)
add_test(NAME application_open_utf8
  COMMAND jwpqt --smoke-test
          "${CMAKE_CURRENT_SOURCE_DIR}/data/sample-utf8.txt"
)
set_tests_properties(application_startup application_open_utf8 PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  TIMEOUT 10
)
