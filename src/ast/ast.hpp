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
};

struct TypeAnnotation {
    bool has_type = false;
    ValueType type = ValueType::int_type;
};

struct Parameter {
    std::string name;
    TypeAnnotation type;
    SourceLocation location;
};

enum class ExpressionKind {
    identifier,
    number,
    boolean,
    binary,
    call,
};

struct Expression {
    ExpressionKind kind;
    std::string lexeme;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    std::vector<std::unique_ptr<Expression>> arguments;
    SourceLocation location;
};

enum class StatementKind {
    let,
    assign,
    print,
    block,
    if_statement,
    while_statement,
    function,
    return_statement,
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
