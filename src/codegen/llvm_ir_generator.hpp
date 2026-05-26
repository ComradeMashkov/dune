#pragma once

#include "ast/ast.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace dune {

class LlvmIrGenerator {
public:
    void generate(const Program& program, std::ostream& output);

private:
    struct FunctionSignature {
        std::vector<ValueType> parameters;
        ValueType return_type = ValueType::int_type;
    };

    struct Local {
        std::string pointer;
        ValueType type = ValueType::int_type;
    };

    struct TypedValue {
        std::string name;
        ValueType type = ValueType::int_type;
    };

    bool emit_statement(const Statement& statement, std::ostream& output);
    bool emit_statements(const std::vector<Statement>& statements, std::ostream& output);
    TypedValue emit_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_binary_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_call_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_text_literal(const std::string& lexeme);
    void emit_function(const Statement& statement, std::ostream& output);
    void emit_print(const TypedValue& value, std::ostream& output);
    void collect_functions(const Program& program);
    void collect_function(const Statement& statement);

    std::string next_register();
    std::string next_label(std::string_view name);
    std::string llvm_type(ValueType type) const;
    std::string printf_format_name(ValueType type) const;
    std::string function_name(const std::string& name) const;
    std::string default_value(ValueType type) const;
    std::string decode_glyph_literal(const std::string& lexeme) const;
    std::string decode_text_literal(const std::string& lexeme) const;
    std::string llvm_text_literal(const std::string& value) const;
    TypedValue cast_for_print(const TypedValue& value, std::ostream& output);

    std::unordered_map<const Expression*, ValueType> expression_types_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, Local> locals_;
    std::vector<std::string> string_globals_;
    ValueType current_return_type_ = ValueType::int_type;
    std::size_t register_count_ = 0;
    std::size_t label_count_ = 0;
    std::size_t string_literal_count_ = 0;
};

} // namespace dune
