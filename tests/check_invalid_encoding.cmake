execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
          "${JWPQT_EXECUTABLE}" --smoke-test --encoding invalid
  RESULT_VARIABLE result
  ERROR_VARIABLE error_output
)

if(NOT result EQUAL 2)
  message(FATAL_ERROR "Expected exit status 2, got ${result}")
endif()
if(NOT error_output MATCHES "Unsupported text encoding: invalid")
  message(FATAL_ERROR "Missing invalid-encoding diagnostic: ${error_output}")
endif()
