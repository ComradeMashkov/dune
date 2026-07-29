execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook run "${NOTEBOOK_FILE}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "notebook run exited with ${run_result}: ${run_error}")
endif()

file(READ "${EXPECTED_OUTPUT_FILE}" expected_output)
if(NOT "${run_output}" STREQUAL "${expected_output}")
    message(FATAL_ERROR "expected notebook stdout '${expected_output}', got '${run_output}'")
endif()

if(NOT "${run_error}" STREQUAL "")
    message(FATAL_ERROR "expected empty notebook stderr, got '${run_error}'")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook check "${NOTEBOOK_FILE}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)

if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "notebook check exited with ${check_result}: ${check_error}")
endif()

if(NOT "${check_output}" MATCHES "notebook outputs are current")
    message(FATAL_ERROR "notebook check did not confirm saved outputs: ${check_output}")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook export "${NOTEBOOK_FILE}" --html -o "${HTML_OUTPUT_FILE}"
    RESULT_VARIABLE export_result
    OUTPUT_VARIABLE export_output
    ERROR_VARIABLE export_error
)

if(NOT export_result EQUAL 0)
    message(FATAL_ERROR "notebook export exited with ${export_result}: ${export_error}")
endif()

file(READ "${HTML_OUTPUT_FILE}" html)
if(NOT "${html}" MATCHES "Dune Notebook" OR NOT "${html}" MATCHES "Scientific workflow")
    message(FATAL_ERROR "notebook HTML export is missing expected content")
endif()

file(REMOVE "${NEW_NOTEBOOK_FILE}")
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook new "${NEW_NOTEBOOK_FILE}" --title "New notebook"
    RESULT_VARIABLE new_result
    OUTPUT_VARIABLE new_output
    ERROR_VARIABLE new_error
)

if(NOT new_result EQUAL 0)
    message(FATAL_ERROR "notebook new exited with ${new_result}: ${new_error}")
endif()

file(READ "${NEW_NOTEBOOK_FILE}" new_notebook)
if(NOT "${new_notebook}" MATCHES "\"dune_notebook\": 1" OR
   NOT "${new_notebook}" MATCHES "\"title\": \"New notebook\"")
    message(FATAL_ERROR "notebook new did not create a versioned .dnb document")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook check "${NEW_NOTEBOOK_FILE}"
    RESULT_VARIABLE new_check_result
    ERROR_VARIABLE new_check_error
)

if(NOT new_check_result EQUAL 0)
    message(FATAL_ERROR "new notebook did not pass notebook check: ${new_check_error}")
endif()

configure_file("${NOTEBOOK_FILE}" "${UPDATE_NOTEBOOK_FILE}" COPYONLY)
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook run "${UPDATE_NOTEBOOK_FILE}" --update
    RESULT_VARIABLE update_result
    OUTPUT_VARIABLE update_output
    ERROR_VARIABLE update_error
)

if(NOT update_result EQUAL 0)
    message(FATAL_ERROR "notebook run --update exited with ${update_result}: ${update_error}")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" notebook check "${UPDATE_NOTEBOOK_FILE}"
    RESULT_VARIABLE updated_check_result
    ERROR_VARIABLE updated_check_error
)

if(NOT updated_check_result EQUAL 0)
    message(FATAL_ERROR "updated notebook did not pass notebook check: ${updated_check_error}")
endif()
