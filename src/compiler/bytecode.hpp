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
    tuple,
    record,
    variant,
    callable,
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
    std::shared_ptr<std::vector<Value>> tuple_value;
    std::shared_ptr<std::vector<Value>> record_value;
    std::size_t variant_tag = 0;
    std::shared_ptr<Value> variant_payload;
    // For `callable`: index into Bytecode::functions of the referenced function.
    std::size_t function_index = 0;
};

enum class OpCode {
    push_constant,
    load_local,
    store_local,
    add,
    subtract,
    multiply,
    divide,
    modulo,
    negate,
    not_value,
    cast_signed,
    cast_unsigned,
    cast_real,
    cast_bool,
    cast_glyph,
    equal,
    not_equal,
    greater,
    greater_equal,
    less,
    less_equal,
    jump_if_false,
    jump,
    call,
    push_function,
    call_value,
    return_value,
    pop,
    make_array,
    make_tuple,
    make_record,
    make_variant,
    make_unit_variant,
    load_variant_tag,
    load_variant_payload,
    load_index,
    load_tuple_element,
    load_field,
    store_index,
    store_field,
    load_slice,
    array_len,
    array_push,
    array_pop,
    array_clear,
    array_is_empty,
    array_contains,
    text_len,
    text_contains,
    text_in,
    text_starts_with,
    text_is_empty,
    read_file,
    write_file,
    stdout_write,
    stderr_write,
    stdout_flush,
    stderr_flush,
    stdin_read_line,
    env_get,
    process_args,
    process_cwd,
    log_emit,
    log_set_level,
    log_level,
    format_text,
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
        std::string extern_symbol;
        std::size_t arity = 0;
        std::size_t local_count = 0;
        std::vector<Instruction> instructions;
        bool is_extern = false;
    };

    std::vector<Function> functions;

    // A `test "name" { ... }` block, compiled as a zero-arg function chunk in
    // `functions`. Run one-by-one by `dune test`, never during normal execution.
    struct Test {
        std::string name;
        std::size_t function_index = 0;
    };

    std::vector<Test> tests;
};

} // namespace dune
