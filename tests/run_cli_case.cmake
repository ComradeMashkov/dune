execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune exited with ${result}: ${error}")
endif()

set(expected_output "${EXPECTED_OUTPUT}\n")
if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected '${expected_output}', got '${output}'")
endif()
