# Verifies the non-TTY (piped / CI) build progress fallback: plain, ANSI-free
# phase lines on stderr and a clean, unchanged exit code. Color is forced off so
# the output must contain no escape sequences at all.
file(REMOVE "${NATIVE_FILE}" "${NATIVE_FILE}.ll")

execute_process(
    COMMAND
        ${CMAKE_COMMAND} -E env DUNE_COLOR=never
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

# No spinner, cursor codes, or colors when stdout/stderr is not a terminal.
string(FIND "${error}" "${escape}" escape_index)
if(NOT escape_index EQUAL -1)
    message(FATAL_ERROR "expected no ANSI escapes in plain build output, got '${error}'")
endif()

function(expect_part expected)
    string(FIND "${error}" "${expected}" index)
    if(index EQUAL -1)
        message(FATAL_ERROR "expected plain build output containing '${expected}', got '${error}'")
    endif()
endfunction()

expect_part("[done]")
expect_part("read source")
expect_part("type check")
expect_part("compile native")
# Every resolved phase carries an elapsed-time suffix such as "(0.0s)".
expect_part("s)")
