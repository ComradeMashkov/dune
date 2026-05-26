#pragma once

#include <memory>
#include <string>
#include <vector>

namespace dune {

enum class ExpressionKind {
    identifier,
    number,
    boolean,
    binary,
};

struct Expression {
    ExpressionKind kind;
    std::string lexeme;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

enum class StatementKind {
    let,
    assign,
    print,
    block,
    if_statement,
    while_statement,
};

struct Statement {
    StatementKind kind;
    std::string name;
    std::unique_ptr<Expression> expression;
    std::vector<Statement> body;
    std::vector<Statement> else_body;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace dune
