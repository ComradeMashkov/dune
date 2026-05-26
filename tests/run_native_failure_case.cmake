file(REMOVE "${NATIVE_FILE}" "${NATIVE_FILE}.ll")

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" build "${SOURCE_FILE}" -o "${NATIVE_FILE}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "dune build exited with ${build_result}: ${build_error}")
endif()

if(NOT EXISTS "${NATIVE_FILE}")
    message(FATAL_ERROR "expected native output file '${NATIVE_FILE}'")
endif()

execute_process(
    COMMAND "${NATIVE_FILE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)

if(run_result EQUAL 0)
    message(FATAL_ERROR "expected native output to fail, got success with '${run_output}'")
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

set(combined_output "${run_output}${run_error}")
string(FIND "${combined_output}" "${expected_error}" expected_error_index)
if(expected_error_index EQUAL -1)
    message(FATAL_ERROR "expected error containing '${expected_error}', got '${combined_output}'")
endif()
