#pragma once

#include "ast/ast.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace dune {

class TypeChecker {
public:
    void check(const Program& program);
    const std::unordered_map<const Expression*, ValueType>& expression_types() const;

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
    ValueType check_expression(const Expression& expression, const TypeAnnotation& expected = {});
    ValueType check_binary_expression(const Expression& expression, const TypeAnnotation& expected);
    ValueType check_call_expression(const Expression& expression);

    bool statement_returns(const Statement& statement) const;
    bool statements_return(const std::vector<Statement>& statements) const;

    ValueType annotation_or_default(const TypeAnnotation& annotation) const;
    bool is_signed_type(ValueType type) const;
    bool is_integer_type(ValueType type) const;
    bool is_unsigned_type(ValueType type) const;
    bool is_real_type(ValueType type) const;
    bool is_numeric_type(ValueType type) const;
    bool can_coerce_integer_literal(const Expression& expression, ValueType target) const;
    void check_integer_literal_range(const Expression& expression, ValueType target) const;
    unsigned long long max_integer_literal(ValueType target) const;
    ValueType coerce_numeric_literal(const Expression& expression, ValueType actual, ValueType target);
    void expect_type(ValueType expected, ValueType actual, SourceLocation location) const;
    std::string diagnostic(SourceLocation location, const std::string& message) const;

    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, ValueType> variables_;
    std::unordered_map<const Expression*, ValueType> expression_types_;
    const FunctionSignature* current_function_ = nullptr;
};

std::string type_name(ValueType type);

} // namespace dune
