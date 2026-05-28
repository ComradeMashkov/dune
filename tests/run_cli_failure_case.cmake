execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected dune output to fail, got success with '${output}'")
endif()

set(expected_error "${EXPECTED_ERROR}")
string(LENGTH "${expected_error}" expected_length)
if(expected_length GREATER_EQUAL 2)
    math(EXPR last_index "${expected_length} - 1")
    string(SUBSTRING "${expected_error}" 0 1 first_char)
    string(SUBSTRING "${expected_error}" ${last_index} 1 last_char)
    if("${first_char}" STREQUAL "\"" AND "${last_char}" STREQUAL "\"")
        math(EXPR inner_length "${expected_length} - 2")
        string(SUBSTRING "${expected_error}" 1 ${inner_length} expected_error)
    endif()
endif()

set(combined_output "${output}${error}")
string(FIND "${combined_output}" "${expected_error}" expected_error_index)
if(expected_error_index EQUAL -1)
    message(FATAL_ERROR "expected error containing '${expected_error}', got '${combined_output}'")
endif()
