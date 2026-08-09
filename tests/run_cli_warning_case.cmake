execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "expected warning-only program to succeed, got ${result}: ${error}")
endif()

file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)
if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "warning program output mismatch: '${output}'")
endif()

string(FIND "${error}" "${EXPECTED_WARNING}" warning_index)
if(warning_index EQUAL -1)
    message(FATAL_ERROR "expected warning containing '${EXPECTED_WARNING}', got '${error}'")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "${SOURCE_FILE}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)

if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "expected dune check with warnings to succeed, got ${check_result}: ${check_error}")
endif()

string(FIND "${check_error}" "${EXPECTED_WARNING}" check_warning_index)
if(check_warning_index EQUAL -1)
    message(FATAL_ERROR "expected dune check warning containing '${EXPECTED_WARNING}', got '${check_error}'")
endif()
