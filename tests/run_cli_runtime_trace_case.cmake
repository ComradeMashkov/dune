execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "${SOURCE_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected runtime panic, got success with '${output}'")
endif()

foreach(needle
        "panic: boom"
        "stack trace:"
        "0: inner"
        "${SOURCE_FILE}:4:5"
        "1: outer"
        "${SOURCE_FILE}:8:5"
        "2: <top-level>"
        "${SOURCE_FILE}:11:1")
    string(FIND "${error}" "${needle}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "expected runtime trace to contain '${needle}', got:\n${error}")
    endif()
endforeach()
