cmake_minimum_required(VERSION 3.20)

project(dune VERSION 0.13.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(DUNE_ENABLE_LINT "Run clang-format and clang-tidy checks before building targets" ON)

file(GLOB_RECURSE DUNE_CXX_FORMAT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

file(GLOB_RECURSE DUNE_CXX_TIDY_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

if(DUNE_ENABLE_LINT)
    find_program(CLANG_FORMAT_EXE NAMES clang-format HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

    if(NOT CLANG_FORMAT_EXE)
        message(FATAL_ERROR "clang-format is required when DUNE_ENABLE_LINT=ON")
    endif()

    if(NOT CLANG_TIDY_EXE)
        message(FATAL_ERROR "clang-tidy is required when DUNE_ENABLE_LINT=ON")
    endif()

    add_custom_target(dune_format_check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${DUNE_CXX_FORMAT_FILES}
        COMMENT "Checking C++ formatting"
        VERBATIM
    )

    add_custom_target(dune_tidy_check
        COMMAND ${CLANG_TIDY_EXE} --quiet -p ${CMAKE_CURRENT_BINARY_DIR} ${DUNE_CXX_TIDY_FILES}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
endif()

function(dune_add_lint_dependencies target_name)
    if(DUNE_ENABLE_LINT)
        add_dependencies(${target_name} dune_format_check dune_tidy_check)
    endif()
endfunction()

add_library(dune_lexer
    src/lexer/lexer.cpp
)
dune_add_lint_dependencies(dune_lexer)

target_include_directories(dune_lexer
    PUBLIC
        src
)

add_library(dune_parser
    src/parser/parser.cpp
)
dune_add_lint_dependencies(dune_parser)

target_link_libraries(dune_parser
    PUBLIC
        dune_lexer
)

add_library(dune_typechecker
    src/typechecker/type_checker.cpp
)
dune_add_lint_dependencies(dune_typechecker)

target_link_libraries(dune_typechecker
    PUBLIC
        dune_parser
)

add_library(dune_modules
    src/modules/module_loader.cpp
)
dune_add_lint_dependencies(dune_modules)
target_compile_definitions(dune_modules PRIVATE DUNE_STDLIB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/stdlib")

target_link_libraries(dune_modules
    PUBLIC
        dune_parser
)

add_library(dune_compiler
    src/compiler/compiler.cpp
)
dune_add_lint_dependencies(dune_compiler)

target_link_libraries(dune_compiler
    PUBLIC
        dune_typechecker
)

add_library(dune_lsp
    src/lsp/lsp_server.cpp
)
dune_add_lint_dependencies(dune_lsp)
target_compile_definitions(dune_lsp PRIVATE DUNE_STDLIB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/stdlib")

target_link_libraries(dune_lsp
    PUBLIC
        dune_modules
        dune_typechecker
)

if(APPLE)
    enable_language(OBJCXX)
    set(DUNE_NATIVE_CANVAS_DISPLAY_SOURCE src/vm/native_canvas_display_macos.mm)
else()
    set(DUNE_NATIVE_CANVAS_DISPLAY_SOURCE src/vm/native_canvas_display.cpp)
endif()

add_library(dune_vm
    src/vm/vm.cpp
    ${DUNE_NATIVE_CANVAS_DISPLAY_SOURCE}
)
dune_add_lint_dependencies(dune_vm)

target_link_libraries(dune_vm
    PUBLIC
        dune_compiler
)

if(APPLE)
    target_link_libraries(dune_vm
        PUBLIC
            "-framework Cocoa"
            "-framework WebKit"
    )
endif()

add_library(dune_doc
    src/doc/doc_generator.cpp
)
dune_add_lint_dependencies(dune_doc)

target_link_libraries(dune_doc
    PUBLIC
        dune_typechecker
)

add_library(dune_repl
    src/repl/repl.cpp
)
dune_add_lint_dependencies(dune_repl)

target_include_directories(dune_repl
    PUBLIC
        src
)

target_link_libraries(dune_repl
    PRIVATE
        dune_modules
        dune_vm
)

add_library(dune_notebook
    src/notebook/cli.cpp
    src/notebook/notebook.cpp
    src/notebook/server.cpp
    src/notebook/web_assets.cpp
)
dune_add_lint_dependencies(dune_notebook)

target_include_directories(dune_notebook
    PUBLIC
        src
)

target_link_libraries(dune_notebook
    PRIVATE
        dune_repl
)

if(WIN32)
    target_link_libraries(dune_notebook PRIVATE ws2_32)
endif()

add_executable(dune src/main.cpp)
target_link_libraries(dune PRIVATE dune_doc dune_lsp dune_modules dune_notebook dune_repl dune_vm Threads::Threads)
dune_add_lint_dependencies(dune)

add_executable(diagnostics_tests tests/diagnostics_tests.cpp)
target_link_libraries(diagnostics_tests PRIVATE dune_lexer)
dune_add_lint_dependencies(diagnostics_tests)

add_executable(lexer_tests tests/lexer_tests.cpp)
target_link_libraries(lexer_tests PRIVATE dune_lexer)
dune_add_lint_dependencies(lexer_tests)

add_executable(parser_tests tests/parser_tests.cpp)
target_link_libraries(parser_tests PRIVATE dune_parser)
dune_add_lint_dependencies(parser_tests)

add_executable(compiler_tests tests/compiler_tests.cpp)
target_link_libraries(compiler_tests PRIVATE dune_compiler dune_modules)
dune_add_lint_dependencies(compiler_tests)

add_executable(type_checker_tests tests/type_checker_tests.cpp)
target_link_libraries(type_checker_tests PRIVATE dune_modules dune_typechecker)
# Lets the fixture helper build module search paths that include the standard
# library alongside the category subfolders that hold importable module fixtures.
target_compile_definitions(type_checker_tests PRIVATE
    DUNE_STDLIB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/stdlib"
    DUNE_FIXTURES_PATH="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
)
dune_add_lint_dependencies(type_checker_tests)

add_executable(vm_tests tests/vm_tests.cpp)
target_link_libraries(vm_tests PRIVATE dune_modules dune_vm)
dune_add_lint_dependencies(vm_tests)

add_executable(lsp_tests tests/lsp_tests.cpp)
target_link_libraries(lsp_tests PRIVATE dune_lsp)
dune_add_lint_dependencies(lsp_tests)

add_executable(repl_tests tests/repl_tests.cpp)
target_link_libraries(repl_tests PRIVATE dune_repl)
dune_add_lint_dependencies(repl_tests)

add_executable(notebook_tests tests/notebook_tests.cpp)
target_link_libraries(notebook_tests PRIVATE dune_notebook Threads::Threads)
if(WIN32)
    target_link_libraries(notebook_tests PRIVATE ws2_32)
endif()
dune_add_lint_dependencies(notebook_tests)

enable_testing()

add_test(NAME smoke COMMAND dune --version)
set_tests_properties(smoke PROPERTIES PASS_REGULAR_EXPRESSION "dune 0.13.1")

add_test(NAME diagnostics_tests COMMAND diagnostics_tests)
add_test(NAME lexer_tests COMMAND lexer_tests)
add_test(NAME parser_tests COMMAND parser_tests)
add_test(NAME compiler_tests COMMAND compiler_tests)
add_test(NAME type_checker_tests COMMAND type_checker_tests)
add_test(NAME vm_tests COMMAND vm_tests)
add_test(NAME lsp_tests COMMAND lsp_tests)
add_test(NAME repl_tests COMMAND repl_tests)
add_test(NAME notebook_tests COMMAND notebook_tests)
add_test(NAME cli_check_static_types COMMAND dune check ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/static_types.dn)
add_test(NAME cli_check_type_error COMMAND dune check ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/type_error.dn)
set_tests_properties(cli_check_type_error PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME cli_stdlib_stats
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/statistics_module.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/stats.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

# `dune repl` accepts scripted stdin while preserving bindings, imports, and
# multi-line definitions between entries.
add_test(
    NAME cli_repl
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repl/session.in
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/repl_session.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_repl_case.cmake
)

add_test(
    NAME cli_notebook
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D NOTEBOOK_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/notebooks/scientific_workflow.dnb
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/notebook_scientific_workflow.out
        -D HTML_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_scientific_workflow.html
        -D NEW_NOTEBOOK_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_new.dnb
        -D UPDATE_NOTEBOOK_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_update.dnb
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_notebook_case.cmake
)

add_test(
    NAME cli_notebook_plot_gallery
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D NOTEBOOK_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/notebooks/plot_gallery.dnb
        -D HTML_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_plot_gallery.html
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_notebook_plot_case.cmake
)

add_test(
    NAME cli_notebook_statistical_analysis
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D NOTEBOOK_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/notebooks/statistical_analysis.dnb
        -D UPDATE_BEFORE_CHECK=ON
        -D UPDATED_NOTEBOOK_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_statistical_analysis_checked.dnb
        -D HTML_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_statistical_analysis.html
        -D "EXPECTED_TITLE=Complete stats module reference"
        -D "EXPECTED_HTML_MARKER=<math class=\"latex-math\""
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_notebook_plot_case.cmake
)

add_test(
    NAME cli_notebook_functions_and_closures
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D NOTEBOOK_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/notebooks/functions_and_closures.dnb
        -D UPDATE_BEFORE_CHECK=ON
        -D UPDATED_NOTEBOOK_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_functions_and_closures_checked.dnb
        -D HTML_OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/notebook_functions_and_closures.html
        -D "EXPECTED_TITLE=Functions, lambdas, and closures"
        -D "EXPECTED_HTML_MARKER=Generated by a captured lambda"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_notebook_plot_case.cmake
)

# `dune doc` renders a module's public API to Markdown from the real AST.
add_test(
    NAME cli_doc_sample
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/doc/sample.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/doc_sample.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_doc_case.cmake
)

# `--check` regenerates in memory and must agree with the golden (exit 0).
add_test(
    NAME cli_doc_check
    COMMAND dune doc ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/doc/sample.dn
            --check -o ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/doc_sample.out
)

# `dune test` runs every `test "..." { ... }` block and reports pass/fail.
add_test(
    NAME cli_test_passing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/test/passing.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/test_passing.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_test_case.cmake
)

# A failing assertion must make `dune test` exit non-zero and report the failure.
add_test(
    NAME cli_test_failing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/test/failing.dn
        -D EXPECTED_ERROR=test result: FAILED
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_test_failure_case.cmake
)

# A located compile error renders a Rust-style source snippet with a caret span.
add_test(
    NAME cli_error_snippet
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/type_snippet.dn
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_snippet_case.cmake
)

add_test(
    NAME cli_check_status_error
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/type_error.dn
        -D EXPECTED_ERROR="expected type 'int' but got 'bool'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_status_failure_case.cmake
)

add_test(
    NAME cli_hello
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/hello.dn
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_formatted_print
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/formatted_print.dn
        -D EXPECTED_OUTPUT="Dune v1\nDune v1 Z\ninline 42\nbool=1, glyph=Z, real=2.5\nplain text still works\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_numeric_literals
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/numeric_literals.dn
        -D EXPECTED_OUTPUT="1000000\n255\n165\n123\n10"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_record_defaults
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/record_defaults.dn
        -D EXPECTED_OUTPUT="0.01\n0\nsgd\n0.1\n0\n0.01\n0.9\n0.2\n0\n10\n1\n20\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_string_escapes
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/string_escapes.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/string_escapes.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_invalid_escape
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/invalid_escape.dn
        -D EXPECTED_ERROR="unknown text escape"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_arithmetic
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/arithmetic.dn
        -D EXPECTED_OUTPUT=28
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_control_flow
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/control_flow.dn
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_for_in_ranges
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/for_in_ranges.dn
        -D EXPECTED_OUTPUT="10\n20\n9"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_membership
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/membership.dn
        -D EXPECTED_OUTPUT="1\n0\n1\n1\n0\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/functions.dn
        -D EXPECTED_OUTPUT=17
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_static_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/static_types.dn
        -D EXPECTED_OUTPUT=1
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_many_args
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/many_args.dn
        -D EXPECTED_OUTPUT=45
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_rich_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/rich_types.dn
        -D EXPECTED_OUTPUT="250\n10000000005\n3.75\nQ"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_standard_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/standard_types.dn
        -D EXPECTED_OUTPUT="types\n12\n32000\n41\n9000000000\n5\n6\n3.75\n2.25\nsame"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_arrays_modules
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/arrays_modules.dn
        -D EXPECTED_OUTPUT="4\n4\n25\n49\n2.25\n8\n27\n4\n9\n10\n2.5\n2.5\n3\n2\n3\n3\n8\n0\n1\n0\n1\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_operators_methods
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/operators_methods.dn
        -D EXPECTED_OUTPUT="-17\n-17\n2\n1\n0\n1\n8.5\n65\nB\n0\n0\n3\n2\n1\n13\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_stdlib_primitives
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/stdlib_primitives.dn
        -D EXPECTED_OUTPUT="d\nlanguage\ndune\nlanguage\n3\n2\n4\n9\n1\n2\n4\n2\n9\n5\n9\n1\n8\n2\ndune\n42\n7"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_io_module
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/io_module.dn
        -D INPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/io_module.in
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/io_module.out
        -D EXPECTED_ERROR_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/io_module.err
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_generics_bounds
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/generics_bounds.dn
        -D EXPECTED_OUTPUT="42\ndone\nlang\n144\n2.25\n18\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_plot_features
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/plot_features.dn
        -D EXPECTED_OUTPUT="1\n1\n1\n1\n1\n1\n1\n1\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_choice_printing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/choice_printing.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/choice_printing.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_operator_overloading
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/operator_overloading.dn
        -D EXPECTED_OUTPUT="11\n22\n9\n6\n9\n10\n10"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_type_aliases
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/type_aliases_run.dn
        -D EXPECTED_OUTPUT="3\n5\n9\n12"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_records_stdlib
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/records_stdlib.dn
        -D EXPECTED_OUTPUT="10\n30\n42\n7\n1\n1\n100\n9\n5\nbad\n1\n4\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_static_associated_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/static_associated_functions.dn
        -D EXPECTED_OUTPUT="0.01\n0.2\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_choices_when
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/choices_when.dn
        -D EXPECTED_OUTPUT="30\n7\n31\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_pattern_matching
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/pattern_matching.dn
        -D EXPECTED_OUTPUT="30\n7\ndone\nbad\n3\n8\n11\n24\n99\n20"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_scopes_assignment_targets
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/scopes_assignment_targets.dn
        -D EXPECTED_OUTPUT="2\n1\n3\n5\n1\n9\n8\n7\n11"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_mutability_aliasing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/mutability_aliasing.dn
        -D EXPECTED_OUTPUT="2\n6\n4\n9\n1\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_value_semantics
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/value_semantics.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/value_semantics.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_closures
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/closures.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/closures.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_captured_reassignment
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/captured_reassignment.dn
        -D EXPECTED_ERROR="cannot reassign captured variable 'value'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_const_reassignment
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/const_reassignment.dn
        -D EXPECTED_ERROR="cannot assign to constant 'values'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_this_reassignment
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/this_reassignment.dn
        -D EXPECTED_ERROR="cannot reassign method receiver 'this'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_object_model
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/types/object_model.dn
        -D EXPECTED_OUTPUT="2\n12\n12\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_autograd
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/autograd_demo.dn
        -D EXPECTED_OUTPUT="11\n7\n2\n18\n12\n12\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_matrix
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/matrix_demo.dn
        -D EXPECTED_OUTPUT="5\n9\n32\n3\n1\n6\n13\n9\n10\n2\n6\n5\n3\n5\n-3\n7\n0\n2\n-42\n10\n3\n2\n3\n6\n2\n3\n5\n5\n6\n6\n3\n2\n6\n58\n154\n1\n0\n4\n9\n16\n3\n2\n5\n6\n15\n0\n4\n1\n0\n2\n3\n5\n7\n1\n4\n14\n15\n64\n12\n-2\n2.5\n7\n4\n1.5\n3\n16\n30\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_array_api
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/array_api.dn
        -D EXPECTED_OUTPUT="6\n6\n1\n3\n0\n2\n4\n15\n3\n3\nx\n21\n0\n3\n6\n0\n5\n9\n3\n4\n1\n1\n27\n0\n1\n4"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_module
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/cli_module.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/cli_module.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_dict_set
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/dict_set.dn
        -D EXPECTED_OUTPUT="2\n15\n7\n-1\n1\n0\n2\n22\n1\n0\n1\n0\n2\n1\n0\n2\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_plot_svg
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/plot_svg.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/plot_svg.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_canvas_gui
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/canvas_gui.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/canvas_gui.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_iteration_protocol
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/iteration_protocol.dn
        -D EXPECTED_OUTPUT="3\n6\n2\n4\n6\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_foreknown
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/foreknown.dn
        -D EXPECTED_OUTPUT="1024\n4096\n120\n55"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_text_concat
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/text_concat.dn
        -D EXPECTED_OUTPUT="Hi, Ada!\nreally?\nababab\ncba\nMIXED CASE\nmixed case\na::b::c\n00042\nx,y,z\n3\ntwo"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

# First-class function values / higher-order array pipeline on the VM.
add_test(
    NAME cli_method_chaining
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/method_chaining.dn
        -D EXPECTED_OUTPUT="45\n2\n3\n7\n5\n60\n1\n0\n2\n20"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_regex
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/regex_demo.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/regex_demo.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_bad_callback_signature
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/bad_callback_signature.dn
        -D EXPECTED_ERROR="has no method 'map' matching argument types (fn(int): int)"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

# Modules v2: a `module` declaration, aliased imports, and selective / grouped
# `from ... import` for both a local module (shapes.dn) and the standard library.
add_test(
    NAME cli_modules_v2
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/modules_v2.dn
        -D EXPECTED_OUTPUT="7\n7\n10\n0\n10\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_module_alias_conflict
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/module_alias_conflict.dn
        -D EXPECTED_ERROR="import alias 'm' is already bound to module 'math'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_module_private_symbol
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/module_private_symbol.dn
        -D EXPECTED_ERROR="module 'hidden_module' does not export 'hidden'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_random_demo
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/random_demo.dn
        -D EXPECTED_OUTPUT="705894\n1126542223\n1579310009\n1\n1\n5\n1\n100"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_log_module
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib/logging.dn
        -D EXPECTED_OUTPUT="stdout-ok"
        -D EXPECTED_ERROR="[debug] cache miss\n[info] building target\n[warn] using fallback config\n[error] failed"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_stderr_case.cmake
)

add_test(
    NAME cli_comprehensions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/comprehensions.dn
        -D EXPECTED_OUTPUT="5\n1\n25\n2\n2\n4\n5\n0\n8\n0\n2\nbb\nccc\n3\n1\n2\n3\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_try_operator
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/language/try_operator.dn
        -D EXPECTED_OUTPUT="1\n7\nok\n0\n-1\nnot positive\n0\n-1\nnot positive\n1\n107\nok\n0\n-1\nnot positive"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_matrix_shape_error
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/errors/matrix_shape_error.dn
        -D EXPECTED_ERROR="vector shape mismatch"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_example_hello
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/hello.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/hello.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_matrix_basics
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/matrix_basics.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/matrix_basics.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_static_shapes
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/static_shapes.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/static_shapes.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_vector_stats
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/vector_stats.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/vector_stats.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_collection_pipeline
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/collection_pipeline.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/collection_pipeline.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_functions_and_closures
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/functions_and_closures.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/functions_and_closures.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_geometry_demo
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/geometry_demo.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/geometry_demo.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_documented
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/documented.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/documented.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_cli_demo
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/cli_demo.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/cli_demo.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_linear_regression
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/linear_regression.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/linear_regression.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_statistical_analysis
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/statistical_analysis.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/golden/statistical_analysis.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)
