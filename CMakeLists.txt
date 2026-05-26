cmake_minimum_required(VERSION 3.20)

project(dune LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dune src/main.cpp)

enable_testing()

add_test(NAME smoke COMMAND dune)
