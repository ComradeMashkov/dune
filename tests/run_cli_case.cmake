execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune exited with ${result}: ${error}")
endif()

set(expected_body "${EXPECTED_OUTPUT}")
string(LENGTH "${expected_body}" expected_length)
if(expected_length GREATER_EQUAL 2)
    math(EXPR last_index "${expected_length} - 1")
    string(SUBSTRING "${expected_body}" 0 1 first_char)
    string(SUBSTRING "${expected_body}" ${last_index} 1 last_char)
    if("${first_char}" STREQUAL "\"" AND "${last_char}" STREQUAL "\"")
        math(EXPR inner_length "${expected_length} - 2")
        string(SUBSTRING "${expected_body}" 1 ${inner_length} expected_body)
    endif()
endif()

set(expected_output "${expected_body}\n")
if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected '${expected_output}', got '${output}'")
endif()
