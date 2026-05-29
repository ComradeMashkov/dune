#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dune {

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t length = 1;
};

enum class ValueType {
    int_type,
    bool_type,
    i8_type,
    i16_type,
    i32_type,
    i64_type,
    isize_type,
    u8_type,
    u16_type,
    u32_type,
    u64_type,
    usize_type,
    real32_type,
    real_type,
    glyph_type,
    text_type,
    unit_type,
    array_type,
    generic_type,
    struct_type,
    enum_type,
};

struct Type {
    ValueType kind = ValueType::int_type;
    std::shared_ptr<Type> element;
    std::string name;
    std::vector<Type> arguments;
};

struct TypeAnnotation {
    bool has_type = false;
    Type type;
};

struct Parameter {
    std::string name;
    TypeAnnotation type;
    SourceLocation location;
    bool exported = false;
};

struct GenericParameter {
    std::string name;
    std::string bound;
    SourceLocation location;
};

enum class ExpressionKind {
    identifier,
    number,
    floating,
    character,
    string,
    boolean,
    array,
    struct_literal,
    index,
    slice,
    member,
    unary,
    cast,
    binary,
    when_expression,
    call,
    method_call,
};

struct Expression {
    ExpressionKind kind;
    std::string lexeme;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    std::vector<std::unique_ptr<Expression>> arguments;
    std::vector<std::string> field_names;
    SourceLocation location;
    TypeAnnotation type;
};

enum class StatementKind {
    binding,
    const_statement,
    assign,
    print,
    block,
    if_statement,
    while_statement,
    for_statement,
    break_statement,
    continue_statement,
    function,
    method_block,
    struct_statement,
    enum_statement,
    contract_statement,
    return_statement,
    expression_statement,
    import_statement,
};

struct Statement {
    StatementKind kind;
    std::string name;
    std::unique_ptr<Expression> expression;
    std::vector<Statement> body;
    std::vector<Statement> else_body;
    TypeAnnotation type;
    std::vector<Parameter> parameters;
    std::vector<GenericParameter> generic_parameters;
    std::vector<Type> contracts;
    SourceLocation location;
    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Statement> increment;
    bool exported = false;
    bool is_extern = false;
    bool is_record_member = false;
    bool is_constructor = false;
    bool is_static_record_member = false;
    std::string extern_symbol;
    std::string owner_record;
    std::unique_ptr<Expression> target;
    std::vector<std::unique_ptr<Expression>> arguments;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace dune
