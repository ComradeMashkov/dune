set(project_dir "${CMAKE_CURRENT_BINARY_DIR}/project_model_diagnostics_case")
file(REMOVE_RECURSE "${project_dir}")
file(MAKE_DIRECTORY "${project_dir}/src/app")
file(MAKE_DIRECTORY "${project_dir}/lib-a")
file(MAKE_DIRECTORY "${project_dir}/lib-b")

file(WRITE "${project_dir}/dune.toml"
    "name = \"diagnostics\"\n"
    "sources = [\"src\", \"lib-a\", \"lib-b\"]\n"
)
file(WRITE "${project_dir}/src/app/main.dn" "import shared;\nimport io;\nio.println(shared.answer());\n")
file(WRITE "${project_dir}/lib-a/shared.dn" "export fn answer(): int { return 1; }\n")
file(WRITE "${project_dir}/lib-b/shared.dn" "export fn answer(): int { return 2; }\n")

execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "src/app/main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE ambiguous_result
    OUTPUT_VARIABLE ambiguous_output
    ERROR_VARIABLE ambiguous_error
)
if(ambiguous_result EQUAL 0)
    message(FATAL_ERROR "expected ambiguous project module to fail")
endif()
set(ambiguous_combined "${ambiguous_output}${ambiguous_error}")
if(NOT "${ambiguous_combined}" MATCHES "ambiguous module 'shared'" OR
   NOT "${ambiguous_combined}" MATCHES "lib-a" OR
   NOT "${ambiguous_combined}" MATCHES "lib-b")
    message(FATAL_ERROR "expected both ambiguous module paths, got '${ambiguous_combined}'")
endif()

file(WRITE "${project_dir}/dune.toml"
    "name = \"diagnostics\"\n"
    "name = \"duplicate\"\n"
)
execute_process(
    COMMAND "${DUNE_EXECUTABLE}" check "src/app/main.dn"
    WORKING_DIRECTORY "${project_dir}"
    RESULT_VARIABLE manifest_result
    OUTPUT_VARIABLE manifest_output
    ERROR_VARIABLE manifest_error
)
if(manifest_result EQUAL 0)
    message(FATAL_ERROR "expected invalid project manifest to fail")
endif()
set(manifest_combined "${manifest_output}${manifest_error}")
if(NOT "${manifest_combined}" MATCHES "dune.toml:2: duplicate project manifest key 'name'")
    message(FATAL_ERROR "expected manifest path and line diagnostic, got '${manifest_combined}'")
endif()
