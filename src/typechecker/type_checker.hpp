#pragma once

#include "ast/ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace dune {

class TypeChecker {
public:
    void check(const Program& program);

private:
    struct FunctionSignature {
        std::string name;
        std::vector<ValueType> parameters;
        ValueType return_type = ValueType::int_type;
        SourceLocation location;
    };

    void collect_function(const Statement& statement);
    void check_function(const Statement& statement);
    void check_statement(const Statement& statement);
    void check_statements(const std::vector<Statement>& statements);
    ValueType check_expression(const Expression& expression);
    ValueType check_binary_expression(const Expression& expression);
    ValueType check_call_expression(const Expression& expression);

    bool statement_returns(const Statement& statement) const;
    bool statements_return(const std::vector<Statement>& statements) const;

    ValueType annotation_or_default(const TypeAnnotation& annotation) const;
    void expect_type(ValueType expected, ValueType actual, SourceLocation location) const;
    std::string diagnostic(SourceLocation location, const std::string& message) const;

    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, ValueType> variables_;
    const FunctionSignature* current_function_ = nullptr;
};

std::string type_name(ValueType type);

} // namespace dune
