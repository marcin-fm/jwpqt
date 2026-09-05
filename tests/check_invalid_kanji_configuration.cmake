if(NOT DEFINED JWPQT_EXECUTABLE OR NOT DEFINED TEST_DIRECTORY)
  message(FATAL_ERROR "JWPQT_EXECUTABLE and TEST_DIRECTORY are required")
endif()

set(config_home "${TEST_DIRECTORY}/invalid-kanji-config")
file(REMOVE_RECURSE "${config_home}")
file(MAKE_DIRECTORY "${config_home}/jwpqt/jwpqt")
file(WRITE "${config_home}/jwpqt/jwpqt/settings.ini"
  "[kanjiColor]\npolicy=v1;invalid;010203;1;040506\n")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
          "QT_QPA_PLATFORM=offscreen"
          "XDG_CONFIG_HOME=${config_home}"
          "${JWPQT_EXECUTABLE}" --smoke-test
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if(result EQUAL 0)
  message(FATAL_ERROR
    "jwpqt accepted malformed startup kanji color configuration\n${output}${error}")
endif()

if(NOT error MATCHES "Could not load the kanji color configuration")
  message(FATAL_ERROR
    "jwpqt reported the wrong malformed-configuration error\n${output}${error}")
endif()
