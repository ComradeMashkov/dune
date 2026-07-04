execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune exited with ${result}: ${error}")
endif()

function(strip_wrapping_quotes input output)
    set(value "${${input}}")
    string(LENGTH "${value}" value_length)
    if(value_length GREATER_EQUAL 2)
        math(EXPR last_index "${value_length} - 1")
        string(SUBSTRING "${value}" 0 1 first_char)
        string(SUBSTRING "${value}" ${last_index} 1 last_char)
        if("${first_char}" STREQUAL "\"" AND "${last_char}" STREQUAL "\"")
            math(EXPR inner_length "${value_length} - 2")
            string(SUBSTRING "${value}" 1 ${inner_length} value)
        endif()
    endif()

    set(${output} "${value}" PARENT_SCOPE)
endfunction()

strip_wrapping_quotes(EXPECTED_OUTPUT expected_output_body)
strip_wrapping_quotes(EXPECTED_ERROR expected_error_body)
set(expected_output "${expected_output_body}\n")
set(expected_error "${expected_error_body}\n")

if(NOT "${output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected stdout '${expected_output}', got '${output}'")
endif()

if(NOT "${error}" STREQUAL "${expected_error}")
    message(FATAL_ERROR "expected stderr '${expected_error}', got '${error}'")
endif()
