# Verifies `dune <file>` renders a Rust-style source snippet for a located
# compile error: a non-zero exit plus the `error:` line, the `-->` locator, the
# quoted source line, and a caret underline. Substring checks keep the assertion
# independent of the absolute fixture path baked into the `-->` line.
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected dune to fail, got success with '${output}'")
endif()

set(combined "${output}${error}")
foreach(needle
        "error: expected type 'int' but got 'text'"
        "-->"
        "1 | x: int = \"hello\""
        "^^^^^^^")
    string(FIND "${combined}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "expected snippet to contain '${needle}', got:\n${combined}")
    endif()
endforeach()
