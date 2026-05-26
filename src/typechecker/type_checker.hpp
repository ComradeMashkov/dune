#pragma once

#include "ast/ast.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dune {

class TypeChecker {
public:
    void check(const Program& program);
    const std::unordered_map<const Expression*, Type>& expression_types() const;
    const std::unordered_map<const Expression*, std::string>& resolved_calls() const;

private:
    struct FunctionSignature {
        std::string name;
        std::string key;
        std::vector<Type> parameters;
        Type return_type;
        SourceLocation location;
    };

    void collect_function(const Statement& statement);
    void check_function(const Statement& statement);
    void check_statement(const Statement& statement);
    void check_statements(const std::vector<Statement>& statements);
    Type check_expression(const Expression& expression, const TypeAnnotation& expected = {});
    Type check_binary_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_call_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_method_call_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_member_expression(const Expression& expression);
    Type check_unary_expression(const Expression& expression);
    Type check_cast_expression(const Expression& expression);
    Type check_array_literal(const Expression& expression, const TypeAnnotation& expected);
    Type check_index_expression(const Expression& expression);
    Type check_function_call(const Expression& expression, const std::string& name,
                             const std::vector<std::unique_ptr<Expression>>& arguments, SourceLocation location,
                             const TypeAnnotation& expected = {});
    Type check_array_method_call(const Type& receiver, const Expression& expression);
    Type check_text_method_call(const Type& receiver, const Expression& expression);

    bool statement_returns(const Statement& statement) const;
    bool statements_return(const std::vector<Statement>& statements) const;

    Type annotation_or_default(const TypeAnnotation& annotation) const;
    bool same_type(const Type& left, const Type& right) const;
    bool is_signed_type(ValueType type) const;
    bool is_integer_type(ValueType type) const;
    bool is_unsigned_type(ValueType type) const;
    bool is_real_type(ValueType type) const;
    bool is_numeric_type(const Type& type) const;
    bool is_cast_allowed(const Type& source, const Type& target) const;
    bool can_coerce_integer_literal(const Expression& expression, const Type& target) const;
    void check_integer_literal_range(const Expression& expression, const Type& target) const;
    unsigned long long max_integer_literal(ValueType target) const;
    Type coerce_numeric_literal(const Expression& expression, const Type& actual, const Type& target);
    void expect_type(const Type& expected, const Type& actual, SourceLocation location) const;
    void expect_imported_module(const std::string& module, SourceLocation location) const;
    void collect_known_module(const std::string& name);
    bool is_qualified_module_name(const std::string& name) const;
    bool is_known_module(const std::string& module) const;
    std::string diagnostic(SourceLocation location, const std::string& message) const;

    const FunctionSignature& resolve_overload(const std::string& name,
                                              const std::vector<std::unique_ptr<Expression>>& arguments,
                                              SourceLocation location, const TypeAnnotation& expected);
    const FunctionSignature& find_function_by_key(const std::string& key, SourceLocation location) const;

    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, std::vector<std::string>> overloads_;
    std::unordered_map<std::string, Type> variables_;
    std::unordered_map<std::string, Type> global_constants_;
    std::unordered_set<std::string> constants_;
    std::unordered_set<std::string> known_modules_;
    std::unordered_map<const Expression*, Type> expression_types_;
    std::unordered_map<const Expression*, std::string> resolved_calls_;
    std::unordered_set<std::string> imports_;
    const FunctionSignature* current_function_ = nullptr;
};

std::string type_name(ValueType type);
std::string type_name(const Type& type);
Type make_type(ValueType type);
Type make_array_type(Type element);
std::string type_key(const Type& type);
std::string function_key(const std::string& name, const std::vector<Type>& parameters);

} // namespace dune
