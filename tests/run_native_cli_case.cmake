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

if(NOT EXISTS "${NATIVE_FILE}.ll")
    message(FATAL_ERROR "expected LLVM IR output file '${NATIVE_FILE}.ll'")
endif()

execute_process(
    COMMAND "${NATIVE_FILE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "native output exited with ${run_result}: ${run_error}")
endif()

if(DEFINED EXPECTED_OUTPUT_FILE)
    file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)
else()
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
endif()

if(NOT "${run_output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected '${expected_output}', got '${run_output}'")
endif()
