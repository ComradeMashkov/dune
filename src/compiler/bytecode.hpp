#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dune {

enum class ValueKind {
    signed_integer,
    unsigned_integer,
    real,
    boolean,
    glyph,
    text,
    unit,
    array,
};

struct Value {
    ValueKind kind = ValueKind::signed_integer;
    std::int64_t signed_value = 0;
    std::uint64_t unsigned_value = 0;
    double real_value = 0.0;
    char glyph_value = '\0';
    bool bool_value = false;
    std::string text_value;
    std::shared_ptr<std::vector<Value>> array_value;
};

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
    pop,
    make_array,
    load_index,
    array_len,
    array_push,
    print,
    halt,
};

struct Instruction {
    OpCode op;
    std::size_t operand = 0;
};

struct Bytecode {
    std::vector<Value> constants;
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
