cmake_minimum_required(VERSION 3.20)

project(dune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(dune_lexer
    src/lexer/lexer.cpp
)

target_include_directories(dune_lexer
    PUBLIC
        src
)

add_library(dune_parser
    src/parser/parser.cpp
)

target_link_libraries(dune_parser
    PUBLIC
        dune_lexer
)

add_library(dune_compiler
    src/compiler/compiler.cpp
)

target_link_libraries(dune_compiler
    PUBLIC
        dune_parser
)

add_library(dune_vm
    src/vm/vm.cpp
)

target_link_libraries(dune_vm
    PUBLIC
        dune_compiler
)

add_executable(dune src/main.cpp)
target_link_libraries(dune PRIVATE dune_vm)

add_executable(lexer_tests tests/lexer_tests.cpp)
target_link_libraries(lexer_tests PRIVATE dune_lexer)

add_executable(parser_tests tests/parser_tests.cpp)
target_link_libraries(parser_tests PRIVATE dune_parser)

add_executable(vm_tests tests/vm_tests.cpp)
target_link_libraries(vm_tests PRIVATE dune_vm)

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
