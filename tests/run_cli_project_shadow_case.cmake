set(project_dir "${CMAKE_CURRENT_BINARY_DIR}/project_model_shadow_case")
file(REMOVE_RECURSE "${project_dir}")
file(MAKE_DIRECTORY "${project_dir}/src")

file(WRITE "${project_dir}/dune.toml"
    "name = \"project_model_shadow_case\"\n"
    "sources = [\"src\"]\n"
)

file(WRITE "${project_dir}/src/math.dn"
    "export fn answer(): int {\n"
    "  return 1;\n"
    "}\n"
)

file(WRITE "${project_dir}/src/main.dn"
    "import math;\n"
    "print(math.answer());\n"
)

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "src/main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

if(result EQUAL 0)
    message(FATAL_ERROR "expected project stdlib shadowing check to fail, got success with '${output}'")
endif()

set(combined_output "${output}${error}")
string(FIND "${combined_output}" "standard library module" expected_error_index)
if(expected_error_index EQUAL -1)
    message(FATAL_ERROR "expected stdlib shadowing diagnostic, got '${combined_output}'")
endif()
