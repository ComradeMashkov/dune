# Golden test driver for `dune doc <file>`: runs the doc generator on a fixture
# module and compares its Markdown to a checked-in golden file.
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" doc "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune doc exited with ${result}: ${error}")
endif()

file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)

if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "dune doc output mismatch.\n--- expected ---\n${expected_output}\n--- got ---\n${output}")
endif()
