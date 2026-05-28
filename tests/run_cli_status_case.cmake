file(REMOVE "${NATIVE_FILE}" "${NATIVE_FILE}.ll")

execute_process(
    COMMAND
        ${CMAKE_COMMAND} -E env DUNE_COLOR=always
        "${DUNE_EXECUTABLE}" build "${SOURCE_FILE}" -o "${NATIVE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "dune build exited with ${result}: ${error}")
endif()

if(NOT EXISTS "${NATIVE_FILE}")
    message(FATAL_ERROR "expected native output file '${NATIVE_FILE}'")
endif()

string(ASCII 27 escape)

function(expect_status_part expected)
    string(FIND "${error}" "${expected}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "expected build status containing '${expected}', got '${error}'")
    endif()
endfunction()

expect_status_part("${escape}[32m")
expect_status_part("[done]")
expect_status_part("read source")
expect_status_part("lex")
expect_status_part("parse AST")
expect_status_part("resolve modules")
expect_status_part("type check")
expect_status_part("emit LLVM IR")
expect_status_part("write LLVM IR")
expect_status_part("compile native")
