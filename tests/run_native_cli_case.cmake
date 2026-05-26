file(REMOVE "${NATIVE_FILE}" "${NATIVE_FILE}.s")

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

if(NOT EXISTS "${NATIVE_FILE}.s")
    message(FATAL_ERROR "expected assembly output file '${NATIVE_FILE}.s'")
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

set(expected_output "${EXPECTED_OUTPUT}\n")
if(NOT "${run_output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected '${expected_output}', got '${run_output}'")
endif()
