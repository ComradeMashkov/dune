cmake_minimum_required(VERSION 3.20)

project(dune VERSION 0.13.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DUNE_ENABLE_LINT "Run clang-format and clang-tidy checks before building targets" ON)
option(DUNE_ENABLE_NATIVE "Require clang++ so `dune build` can produce native binaries" ON)

find_program(DUNE_CLANGXX_EXE NAMES clang++ HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

# The native backend shells out to clang++ at runtime. When DUNE_ENABLE_NATIVE
# is OFF the VM, type checker, and LSP still build and run without any LLVM
# toolchain; `dune build` then reports a clear error instead of compiling.
if(DUNE_ENABLE_NATIVE)
    if(NOT DUNE_CLANGXX_EXE)
        message(FATAL_ERROR "clang++ is required for the native LLVM backend; install it or reconfigure with -D DUNE_ENABLE_NATIVE=OFF")
    endif()

    set(DUNE_NATIVE_CLANGXX "${DUNE_CLANGXX_EXE}")
else()
    set(DUNE_NATIVE_CLANGXX "")
endif()

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

add_library(dune_codegen
    src/codegen/llvm_ir_generator.cpp
)
dune_add_lint_dependencies(dune_codegen)

target_link_libraries(dune_codegen
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

add_library(dune_vm
    src/vm/vm.cpp
)
dune_add_lint_dependencies(dune_vm)

target_link_libraries(dune_vm
    PUBLIC
        dune_compiler
)

add_executable(dune src/main.cpp)
target_link_libraries(dune PRIVATE dune_codegen dune_lsp dune_modules dune_vm)
target_compile_definitions(dune PRIVATE DUNE_CLANGXX_PATH="${DUNE_NATIVE_CLANGXX}")
dune_add_lint_dependencies(dune)

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
dune_add_lint_dependencies(type_checker_tests)

add_executable(vm_tests tests/vm_tests.cpp)
target_link_libraries(vm_tests PRIVATE dune_modules dune_vm)
dune_add_lint_dependencies(vm_tests)

add_executable(lsp_tests tests/lsp_tests.cpp)
target_link_libraries(lsp_tests PRIVATE dune_lsp)
dune_add_lint_dependencies(lsp_tests)

enable_testing()

add_test(NAME smoke COMMAND dune --version)
set_tests_properties(smoke PROPERTIES PASS_REGULAR_EXPRESSION "dune 0.13.1")

add_test(NAME lexer_tests COMMAND lexer_tests)
add_test(NAME parser_tests COMMAND parser_tests)
add_test(NAME compiler_tests COMMAND compiler_tests)
add_test(NAME type_checker_tests COMMAND type_checker_tests)
add_test(NAME vm_tests COMMAND vm_tests)
add_test(NAME lsp_tests COMMAND lsp_tests)
add_test(NAME cli_check_static_types COMMAND dune check ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static_types.dn)
add_test(NAME cli_check_type_error COMMAND dune check ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/type_error.dn)
set_tests_properties(cli_check_type_error PROPERTIES WILL_FAIL TRUE)

add_test(
    NAME cli_build_native_status
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hello.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/status_hello_native
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_status_case.cmake
)

add_test(
    NAME cli_check_status_error
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/type_error.dn
        -D EXPECTED_ERROR="expected type 'int' but got 'bool'"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_status_failure_case.cmake
)

add_test(
    NAME cli_hello
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hello.dn
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_formatted_print
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/formatted_print.dn
        -D EXPECTED_OUTPUT="Dune v1\nDune v1 Z\ninline 42\nbool=1, glyph=Z, real=2.5\nplain text still works\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_numeric_literals
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/numeric_literals.dn
        -D EXPECTED_OUTPUT="1000000\n255\n165\n123\n10"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_record_defaults
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/record_defaults.dn
        -D EXPECTED_OUTPUT="0.01\n0\nsgd\n0.1\n0\n0.01\n0.9\n0.2\n0\n10\n1\n20\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_string_escapes
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/string_escapes.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/string_escapes.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_invalid_escape
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/invalid_escape.dn
        -D EXPECTED_ERROR="unknown text escape"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_arithmetic
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arithmetic.dn
        -D EXPECTED_OUTPUT=28
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_control_flow
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/control_flow.dn
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_for_in_ranges
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/for_in_ranges.dn
        -D EXPECTED_OUTPUT="10\n20\n9"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_membership
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/membership.dn
        -D EXPECTED_OUTPUT="1\n0\n1\n1\n0\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/functions.dn
        -D EXPECTED_OUTPUT=17
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_static_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static_types.dn
        -D EXPECTED_OUTPUT=1
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_many_args
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/many_args.dn
        -D EXPECTED_OUTPUT=45
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_rich_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rich_types.dn
        -D EXPECTED_OUTPUT="250\n10000000005\n3.75\nQ"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_standard_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/standard_types.dn
        -D EXPECTED_OUTPUT="types\n12\n32000\n41\n9000000000\n5\n6\n3.75\n2.25\nsame"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_arrays_modules
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arrays_modules.dn
        -D EXPECTED_OUTPUT="4\n4\n25\n49\n2.25\n8\n27\n4\n9\n10\n2.5\n2.5\n3\n2\n3\n3\n8\n0\n1\n0\n1\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_operators_methods
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/operators_methods.dn
        -D EXPECTED_OUTPUT="-17\n-17\n2\n1\n0\n1\n8.5\n65\nB\n0\n0\n3\n2\n1\n13\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_stdlib_primitives
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib_primitives.dn
        -D EXPECTED_OUTPUT="d\nlanguage\ndune\nlanguage\n3\n2\n4\n9\n1\n2\n4\n2\n9\n5\n9\n1\n8\n2\ndune\n42\n7"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_generics_bounds
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/generics_bounds.dn
        -D EXPECTED_OUTPUT="42\ndone\nlang\n144\n2.25\n18"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_type_aliases
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/type_aliases_run.dn
        -D EXPECTED_OUTPUT="3\n5"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_records_stdlib
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/records_stdlib.dn
        -D EXPECTED_OUTPUT="10\n30\n42\n7\n1\n1\n100\n9\n5\nbad\n1\n4\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_static_associated_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static_associated_functions.dn
        -D EXPECTED_OUTPUT="0.01\n0.2\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_choices_when
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/choices_when.dn
        -D EXPECTED_OUTPUT="30\n7\n31\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_pattern_matching
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pattern_matching.dn
        -D EXPECTED_OUTPUT="30\n7\ndone\nbad\n3\n8\n11\n24\n99\n20"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_scopes_assignment_targets
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scopes_assignment_targets.dn
        -D EXPECTED_OUTPUT="2\n1\n3\n5\n1\n9\n8\n7\n11"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_mutability_aliasing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mutability_aliasing.dn
        -D EXPECTED_OUTPUT="2\n6\n4\n9\n1\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_object_model
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/object_model.dn
        -D EXPECTED_OUTPUT="2\n12\n12\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_autograd
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/autograd_demo.dn
        -D EXPECTED_OUTPUT="11\n7\n2\n18\n12\n12\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_matrix
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_demo.dn
        -D EXPECTED_OUTPUT="5\n9\n32\n3\n1\n6\n13\n9\n10\n2\n6\n5\n3\n5\n-3\n7\n0\n2\n-42\n10\n3\n2\n3\n6\n2\n3\n5\n5\n6\n6\n3\n2\n6\n58\n154\n1\n0\n4\n9\n16\n3\n2\n5\n6\n15\n0\n4\n1\n0\n2\n3\n5\n7\n1\n4\n14\n15\n64\n12\n-2\n2.5\n7\n4\n1.5\n3\n16\n30\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_array_api
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/array_api.dn
        -D EXPECTED_OUTPUT="6\n6\n1\n3\n0\n2\n4\n15\n3\n3\nx\n21\n0\n3\n6\n0\n5\n9\n3\n4\n1\n1\n27\n0\n1\n4"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_dict_set
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dict_set.dn
        -D EXPECTED_OUTPUT="2\n15\n7\n-1\n1\n0\n2\n22\n1\n0\n1\n0\n2\n1\n0\n2\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_random_demo
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/random_demo.dn
        -D EXPECTED_OUTPUT="705894\n1126542223\n1579310009\n1\n1\n5\n1\n100"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_comprehensions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/comprehensions.dn
        -D EXPECTED_OUTPUT="5\n1\n25\n2\n2\n4\n5\n0\n8\n0\n2\nbb\nccc\n3\n1\n2\n3\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_try_operator
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/try_operator.dn
        -D EXPECTED_OUTPUT="1\n7\nok\n0\n-1\nnot positive\n0\n-1\nnot positive\n1\n107\nok\n0\n-1\nnot positive"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_matrix_shape_error
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_shape_error.dn
        -D EXPECTED_ERROR="vector shape mismatch"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_failure_case.cmake
)

add_test(
    NAME cli_example_matrix_basics
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/matrix_basics.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/matrix_basics.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_vector_stats
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/vector_stats.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/vector_stats.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_example_linear_regression
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/linear_regression.dn
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/linear_regression.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_build_native_hello
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hello.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/hello_native
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_formatted_print
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/formatted_print.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/formatted_print_native
        -D EXPECTED_OUTPUT="Dune v1\nDune v1 Z\ninline 42\nbool=1, glyph=Z, real=2.5\nplain text still works\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_numeric_literals
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/numeric_literals.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/numeric_literals_native
        -D EXPECTED_OUTPUT="1000000\n255\n165\n123\n10"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_string_escapes
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/string_escapes.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/string_escapes_native
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/string_escapes.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_record_defaults
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/record_defaults.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/record_defaults_native
        -D EXPECTED_OUTPUT="0.01\n0\nsgd\n0.1\n0\n0.01\n0.9\n0.2\n0\n10\n1\n20\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/functions.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/functions_native
        -D EXPECTED_OUTPUT=17
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_many_args
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/many_args.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/many_args_native
        -D EXPECTED_OUTPUT=45
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_rich_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/rich_types.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/rich_types_native
        -D EXPECTED_OUTPUT="250\n10000000005\n3.75\nQ"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_standard_types
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/standard_types.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/standard_types_native
        -D EXPECTED_OUTPUT="types\n12\n32000\n41\n9000000000\n5\n6\n3.75\n2.25\nsame"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_arrays_modules
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arrays_modules.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/arrays_modules_native
        -D EXPECTED_OUTPUT="4\n4\n25\n49\n2.25\n8\n27\n4\n9\n10\n2.5\n2.5\n3\n2\n3\n3\n8\n0\n1\n0\n1\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_operators_methods
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/operators_methods.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/operators_methods_native
        -D EXPECTED_OUTPUT="-17\n-17\n2\n1\n0\n1\n8.5\n65\nB\n0\n0\n3\n2\n1\n13\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_stdlib_primitives
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/stdlib_primitives.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/stdlib_primitives_native
        -D EXPECTED_OUTPUT="d\nlanguage\ndune\nlanguage\n3\n2\n4\n9\n1\n2\n4\n2\n9\n5\n9\n1\n8\n2\ndune\n42\n7"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_generics_bounds
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/generics_bounds.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/generics_bounds_native
        -D EXPECTED_OUTPUT="42\ndone\nlang\n144\n2.25\n18"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_type_aliases
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/type_aliases_run.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/type_aliases_native
        -D EXPECTED_OUTPUT="3\n5"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_records_stdlib
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/records_stdlib.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/records_stdlib_native
        -D EXPECTED_OUTPUT="10\n30\n42\n7\n1\n1\n100\n9\n5\nbad\n1\n4\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_static_associated_functions
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/static_associated_functions.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/static_associated_functions_native
        -D EXPECTED_OUTPUT="0.01\n0.2\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_choices_when
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/choices_when.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/choices_when_native
        -D EXPECTED_OUTPUT="30\n7\n31\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_pattern_matching
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pattern_matching.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/pattern_matching_native
        -D EXPECTED_OUTPUT="30\n7\ndone\nbad\n3\n8\n11\n24\n99\n20"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_scopes_assignment_targets
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/scopes_assignment_targets.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/scopes_assignment_targets_native
        -D EXPECTED_OUTPUT="2\n1\n3\n5\n1\n9\n8\n7\n11"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_mutability_aliasing
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mutability_aliasing.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/mutability_aliasing_native
        -D EXPECTED_OUTPUT="2\n6\n4\n9\n1\n2"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_object_model
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/object_model.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/object_model_native
        -D EXPECTED_OUTPUT="2\n12\n12\n42"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_autograd
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/autograd_demo.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/autograd_native
        -D EXPECTED_OUTPUT="11\n7\n2\n18\n12\n12\n0\n0"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_matrix
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_demo.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/matrix_native
        -D EXPECTED_OUTPUT="5\n9\n32\n3\n1\n6\n13\n9\n10\n2\n6\n5\n3\n5\n-3\n7\n0\n2\n-42\n10\n3\n2\n3\n6\n2\n3\n5\n5\n6\n6\n3\n2\n6\n58\n154\n1\n0\n4\n9\n16\n3\n2\n5\n6\n15\n0\n4\n1\n0\n2\n3\n5\n7\n1\n4\n14\n15\n64\n12\n-2\n2.5\n7\n4\n1.5\n3\n16\n30\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_array_api
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/array_api.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/array_api_native
        -D EXPECTED_OUTPUT="6\n6\n1\n3\n0\n2\n4\n15\n3\n3\nx\n21\n0\n3\n6\n0\n5\n9\n3\n4\n1\n1\n27\n0\n1\n4"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_dict_set
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dict_set.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/dict_set_native
        -D EXPECTED_OUTPUT="2\n15\n7\n-1\n1\n0\n2\n22\n1\n0\n1\n0\n2\n1\n0\n2\n1\n1\n1"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_random_demo
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/random_demo.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/random_demo_native
        -D EXPECTED_OUTPUT="705894\n1126542223\n1579310009\n1\n1\n5\n1\n100"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_bounds_check
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/native_bounds_error.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/native_bounds_error
        -D EXPECTED_ERROR="array index out of bounds"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_failure_case.cmake
)

add_test(
    NAME cli_build_native_matrix_shape_error
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/matrix_shape_error.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/matrix_shape_error_native
        -D EXPECTED_ERROR="vector shape mismatch"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_failure_case.cmake
)

add_test(
    NAME cli_build_native_example_matrix_basics
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/matrix_basics.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/example_matrix_basics_native
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/matrix_basics.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_example_vector_stats
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/vector_stats.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/example_vector_stats_native
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/vector_stats.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_example_linear_regression
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/examples/linear_regression.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/example_linear_regression_native
        -D EXPECTED_OUTPUT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/golden/linear_regression.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_control_flow
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/control_flow.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/control_flow_native
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_for_in_ranges
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/for_in_ranges.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/for_in_ranges_native
        -D EXPECTED_OUTPUT="10\n20\n9"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)

add_test(
    NAME cli_build_native_membership
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/membership.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/membership_native
        -D EXPECTED_OUTPUT="1\n0\n1\n1\n0\n99"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)
