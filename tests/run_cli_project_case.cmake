set(project_dir "${CMAKE_CURRENT_BINARY_DIR}/project_model_case")
file(REMOVE_RECURSE "${project_dir}")
file(MAKE_DIRECTORY "${project_dir}/src/app")
file(MAKE_DIRECTORY "${project_dir}/tests/specs")

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
    "import io;\n"
    "io.println(helper.answer());\n"
)

file(WRITE "${project_dir}/tests/test_helper.dn"
    "export fn offset(): int {\n"
    "  return 1;\n"
    "}\n"
)

file(WRITE "${project_dir}/tests/specs/project_test.dn"
    "import helper;\n"
    "import test_helper;\n"
    "import runtime;\n"
    "@test fn resolves_project_roots(): unit {\n"
    "  if helper.answer() + test_helper.offset() != 43 { runtime.panic(\"wrong project result\"); }\n"
    "}\n"
)

file(WRITE "${project_dir}/src/app/production_cannot_import_tests.dn"
    "import test_helper;\n"
    "import io;\n"
    "io.println(test_helper.offset());\n"
)

file(WRITE "${project_dir}/root_helper.dn"
    "export fn answer(): int {\n"
    "  return 7;\n"
    "}\n"
)

file(WRITE "${project_dir}/main.dn"
    "import root_helper;\n"
    "import io;\n"
    "io.println(root_helper.answer());\n"
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
    COMMAND "${DUNE_EXECUTABLE}" check "main.dn"
    WORKING_DIRECTORY "${project_dir}/src/app"
    RESULT_VARIABLE nested_check_result
    OUTPUT_VARIABLE nested_check_output
    ERROR_VARIABLE nested_check_error
)

if(NOT nested_check_result EQUAL 0)
    message(FATAL_ERROR
        "dune check from nested project directory exited with ${nested_check_result}: "
        "${nested_check_error}${nested_check_output}"
    )
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
    COMMAND "${DUNE_EXECUTABLE}" test "tests/specs/project_test.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "dune test exited with ${test_result}: ${test_error}${test_output}")
endif()

if(NOT "${test_output}" MATCHES "1 passed; 0 failed")
    message(FATAL_ERROR "expected project test to pass, got '${test_output}'")
endif()

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "src/app/production_cannot_import_tests.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE production_result
    OUTPUT_VARIABLE production_output
    ERROR_VARIABLE production_error
)

if(production_result EQUAL 0)
    message(FATAL_ERROR "expected production source importing a test helper to fail")
endif()

set(production_combined "${production_output}${production_error}")
if(NOT "${production_combined}" MATCHES "unknown module 'test_helper'")
    message(FATAL_ERROR "expected isolated test-root diagnostic, got '${production_combined}'")
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
