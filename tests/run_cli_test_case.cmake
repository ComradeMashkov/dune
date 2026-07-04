# Golden test driver for `dune test <file>`: runs the test runner on a fixture
# module and compares its report to a checked-in golden file.
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" test "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune test exited with ${result}: ${error}")
endif()

file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)

if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "dune test output mismatch.\n--- expected ---\n${expected_output}\n--- got ---\n${output}")
endif()
