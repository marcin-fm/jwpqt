execute_process(
  COMMAND "${JWPQT_EXECUTABLE}" --smoke-test "${ASCII_FILE}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
  TIMEOUT 5
)

if(NOT result EQUAL 1)
  message(FATAL_ERROR "Expected exit 1, got ${result}: ${output}${error}")
endif()

set(expected
    "Could not open ${ASCII_FILE}: the file type or encoding could not be determined noninteractively")
string(FIND "${error}" "${expected}" found)
if(found EQUAL -1)
  message(FATAL_ERROR "Expected noninteractive diagnostic, got: ${error}")
endif()

execute_process(
  COMMAND "${JWPQT_EXECUTABLE}" --smoke-test --encoding utf-8 "${EUC_FILE}"
  RESULT_VARIABLE explicit_result
  OUTPUT_VARIABLE explicit_output
  ERROR_VARIABLE explicit_error
  TIMEOUT 5
)

if(NOT explicit_result EQUAL 1)
  message(FATAL_ERROR
    "Expected explicit decode exit 1, got ${explicit_result}: ${explicit_output}${explicit_error}")
endif()

set(explicit_expected
    "Could not open ${EUC_FILE}: the requested encoding failed")
string(FIND "${explicit_error}" "${explicit_expected}" explicit_found)
if(explicit_found EQUAL -1)
  message(FATAL_ERROR "Expected explicit-open diagnostic, got: ${explicit_error}")
endif()
