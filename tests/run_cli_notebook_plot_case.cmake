set(notebook_to_check "${NOTEBOOK_FILE}")

# Some showcase notebooks intentionally ship with clear cells so readers can
# execute them interactively from a clean state. Exercise those notebooks on a
# build-directory copy, preserving the source file while still validating every
# result and rich output in CI.
if(UPDATE_BEFORE_CHECK)
    if(NOT DEFINED UPDATED_NOTEBOOK_FILE)
        message(FATAL_ERROR "UPDATED_NOTEBOOK_FILE is required when UPDATE_BEFORE_CHECK is enabled")
    endif()

    configure_file("${NOTEBOOK_FILE}" "${UPDATED_NOTEBOOK_FILE}" COPYONLY)
    execute_process(
        COMMAND "${DUNE_EXECUTABLE}" notebook run "${UPDATED_NOTEBOOK_FILE}" --update
        RESULT_VARIABLE update_result
        OUTPUT_VARIABLE update_output
        ERROR_VARIABLE update_error
    )

    if(NOT update_result EQUAL 0)
        message(FATAL_ERROR "notebook update exited with ${update_result}: ${update_error}")
    endif()

    set(notebook_to_check "${UPDATED_NOTEBOOK_FILE}")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook check "${notebook_to_check}"
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
    COMMAND "${DUNE_EXECUTABLE}" notebook export "${notebook_to_check}" --html -o "${HTML_OUTPUT_FILE}"
    RESULT_VARIABLE export_result
    ERROR_VARIABLE export_error
)

if(NOT export_result EQUAL 0)
    message(FATAL_ERROR "plot notebook export exited with ${export_result}: ${export_error}")
endif()

file(READ "${HTML_OUTPUT_FILE}" html)
if(NOT DEFINED EXPECTED_TITLE)
    set(EXPECTED_TITLE "Plot gallery")
endif()
string(FIND "${html}" "${EXPECTED_TITLE}" title_position)
string(FIND "${html}" "class=\"rich-output\"" output_position)
string(FIND "${html}" "data:image/svg+xml;charset=utf-8," image_position)

if(title_position EQUAL -1 OR output_position EQUAL -1 OR image_position EQUAL -1)
    message(FATAL_ERROR "plot notebook HTML export is missing inline chart output")
endif()

if(DEFINED EXPECTED_HTML_MARKER)
    string(FIND "${html}" "${EXPECTED_HTML_MARKER}" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR "notebook HTML export is missing '${EXPECTED_HTML_MARKER}'")
    endif()
endif()
