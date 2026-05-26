#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace dune {

enum class OpCode {
    push_constant,
    load_local,
    store_local,
    add,
    subtract,
    multiply,
    divide,
    equal,
    not_equal,
    greater,
    greater_equal,
    less,
    less_equal,
    jump_if_false,
    jump,
    call,
    return_value,
    print,
    halt,
};

struct Instruction {
    OpCode op;
    std::size_t operand = 0;
};

struct Bytecode {
    std::vector<int> constants;
    std::vector<Instruction> instructions;
    std::size_t local_count = 0;

    struct Function {
        std::string name;
        std::size_t arity = 0;
        std::size_t local_count = 0;
        std::vector<Instruction> instructions;
    };

    std::vector<Function> functions;
};

} // namespace dune
