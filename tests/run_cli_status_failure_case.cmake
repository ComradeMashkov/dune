execute_process(
    COMMAND
        ${CMAKE_COMMAND} -E env DUNE_COLOR=always
        "${DUNE_EXECUTABLE}" check "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected dune check to fail, got success with '${output}'")
endif()

string(ASCII 27 escape)
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

function(expect_status_part expected)
    string(FIND "${error}" "${expected}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "expected failure status containing '${expected}', got '${error}'")
    endif()
endfunction()

expect_status_part("${escape}[31m")
expect_status_part("[error]")
expect_status_part("type check")
expect_status_part("${expected_error}")
