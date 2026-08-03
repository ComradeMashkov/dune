execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook check "${NOTEBOOK_FILE}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)

if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "plot notebook check exited with ${check_result}: ${check_error}")
endif()

if(NOT "${check_output}" MATCHES "notebook outputs are current")
    message(FATAL_ERROR "plot notebook outputs are stale: ${check_output}")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook export "${NOTEBOOK_FILE}" --html -o "${HTML_OUTPUT_FILE}"
    RESULT_VARIABLE export_result
    ERROR_VARIABLE export_error
)

if(NOT export_result EQUAL 0)
    message(FATAL_ERROR "plot notebook export exited with ${export_result}: ${export_error}")
endif()

file(READ "${HTML_OUTPUT_FILE}" html)
string(FIND "${html}" "Plot gallery" title_position)
string(FIND "${html}" "class=\"rich-output\"" output_position)
string(FIND "${html}" "data:image/svg+xml;charset=utf-8," image_position)

if(title_position EQUAL -1 OR output_position EQUAL -1 OR image_position EQUAL -1)
    message(FATAL_ERROR "plot notebook HTML export is missing inline chart output")
endif()
