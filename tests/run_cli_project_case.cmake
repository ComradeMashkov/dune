set(project_dir "${CMAKE_CURRENT_BINARY_DIR}/project_model_case")
file(REMOVE_RECURSE "${project_dir}")
file(MAKE_DIRECTORY "${project_dir}/src/app")
file(MAKE_DIRECTORY "${project_dir}/tests")

file(WRITE "${project_dir}/dune.toml"
    "name = \"project_model_case\"\n"
    "version = \"0.1.0\"\n"
    "sources = [\"src\"]\n"
    "tests = [\"tests\"]\n"
)

file(WRITE "${project_dir}/src/helper.dn"
    "export fn answer(): int {\n"
    "  return 42;\n"
    "}\n"
)

file(WRITE "${project_dir}/src/app/main.dn"
    "import helper;\n"
    "print(helper.answer());\n"
)

file(WRITE "${project_dir}/root_helper.dn"
    "export fn answer(): int {\n"
    "  return 7;\n"
    "}\n"
)

file(WRITE "${project_dir}/main.dn"
    "import root_helper;\n"
    "print(root_helper.answer());\n"
)

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "src/app/main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)

if(NOT check_result EQUAL 0)
    message(FATAL_ERROR "dune check exited with ${check_result}: ${check_error}${check_output}")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "src/app/main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "dune run exited with ${run_result}: ${run_error}")
endif()

if(NOT "${run_output}" STREQUAL "42\n")
    message(FATAL_ERROR "expected project run output '42', got '${run_output}'")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" "main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE root_run_result
    OUTPUT_VARIABLE root_run_output
    ERROR_VARIABLE root_run_error
)

if(NOT root_run_result EQUAL 0)
    message(FATAL_ERROR "dune root run exited with ${root_run_result}: ${root_run_error}")
endif()

if(NOT "${root_run_output}" STREQUAL "7\n")
    message(FATAL_ERROR "expected project root run output '7', got '${root_run_output}'")
endif()
