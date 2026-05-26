cmake_minimum_required(VERSION 3.20)

project(dune LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(dune_lexer
    src/lexer/lexer.cpp
)

target_include_directories(dune_lexer
    PUBLIC
        src
)

add_executable(dune src/main.cpp)

add_executable(lexer_tests tests/lexer_tests.cpp)
target_link_libraries(lexer_tests PRIVATE dune_lexer)

enable_testing()

add_test(NAME smoke COMMAND dune)
add_test(NAME lexer_tests COMMAND lexer_tests)
