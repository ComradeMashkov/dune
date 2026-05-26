cmake_minimum_required(VERSION 3.20)

project(dune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_library(dune_compiler
    src/compiler/compiler.cpp
)
dune_add_lint_dependencies(dune_compiler)

target_link_libraries(dune_compiler
    PUBLIC
        dune_parser
)

add_library(dune_codegen
    src/codegen/assembly_generator.cpp
)
dune_add_lint_dependencies(dune_codegen)

target_link_libraries(dune_codegen
    PUBLIC
        dune_parser
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
target_link_libraries(dune PRIVATE dune_codegen dune_vm)
dune_add_lint_dependencies(dune)

add_executable(lexer_tests tests/lexer_tests.cpp)
target_link_libraries(lexer_tests PRIVATE dune_lexer)
dune_add_lint_dependencies(lexer_tests)

add_executable(parser_tests tests/parser_tests.cpp)
target_link_libraries(parser_tests PRIVATE dune_parser)
dune_add_lint_dependencies(parser_tests)

add_executable(vm_tests tests/vm_tests.cpp)
target_link_libraries(vm_tests PRIVATE dune_vm)
dune_add_lint_dependencies(vm_tests)

enable_testing()

add_test(NAME smoke COMMAND dune --version)
set_tests_properties(smoke PROPERTIES PASS_REGULAR_EXPRESSION "dune 0.1.0")

add_test(NAME lexer_tests COMMAND lexer_tests)
add_test(NAME parser_tests COMMAND parser_tests)
add_test(NAME vm_tests COMMAND vm_tests)

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
    NAME cli_arithmetic
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/arithmetic.dn
        -D EXPECTED_OUTPUT=28
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_case.cmake
)

add_test(
    NAME cli_build_native
    COMMAND
        ${CMAKE_COMMAND}
        -D DUNE_EXECUTABLE=$<TARGET_FILE:dune>
        -D SOURCE_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/hello.dn
        -D NATIVE_FILE=${CMAKE_CURRENT_BINARY_DIR}/hello_native
        -D EXPECTED_OUTPUT=42
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_native_cli_case.cmake
)
