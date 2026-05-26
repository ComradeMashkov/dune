#pragma once

#include <memory>
#include <string>
#include <vector>

namespace dune {

enum class ExpressionKind {
    identifier,
    number,
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
    print,
};

struct Statement {
    StatementKind kind;
    std::string name;
    std::unique_ptr<Expression> expression;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace dune
