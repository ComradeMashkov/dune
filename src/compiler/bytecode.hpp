#pragma once

#include <cstddef>
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
};

} // namespace dune
