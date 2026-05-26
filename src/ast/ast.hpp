#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dune {

struct SourceLocation {
    std::size_t line = 1;
    std::size_t column = 1;
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
};

struct Type {
    ValueType kind = ValueType::int_type;
    std::shared_ptr<Type> element;
};

struct TypeAnnotation {
    bool has_type = false;
    Type type;
};

struct Parameter {
    std::string name;
    TypeAnnotation type;
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
    index,
    member,
    unary,
    cast,
    binary,
    call,
    method_call,
};

struct Expression {
    ExpressionKind kind;
    std::string lexeme;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    std::vector<std::unique_ptr<Expression>> arguments;
    SourceLocation location;
    TypeAnnotation type;
};

enum class StatementKind {
    let,
    const_statement,
    assign,
    print,
    block,
    if_statement,
    while_statement,
    function,
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
    SourceLocation location;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace dune
