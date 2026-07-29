execute_process(
    COMMAND "${DUNE_EXECUTABLE}" repl
    INPUT_FILE "${INPUT_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune repl exited with ${result}: ${error}")
endif()

file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)
if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected stdout '${expected_output}', got '${output}'")
endif()

if(NOT "${error}" STREQUAL "")
    message(FATAL_ERROR "expected empty stderr, got '${error}'")
endif()
