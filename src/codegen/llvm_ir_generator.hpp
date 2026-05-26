#pragma once

#include "ast/ast.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dune {

class LlvmIrGenerator {
public:
    void generate(const Program& program, std::ostream& output);

private:
    struct FunctionSignature {
        std::vector<Type> parameters;
        Type return_type;
        std::string extern_symbol;
        bool is_extern = false;
    };

    struct Local {
        std::string pointer;
        Type type;
    };

    struct TypedValue {
        std::string name;
        Type type;
    };

    struct LoopLabels {
        std::string break_label;
        std::string continue_label;
    };

    bool emit_statement(const Statement& statement, std::ostream& output);
    bool emit_statements(const std::vector<Statement>& statements, std::ostream& output);
    TypedValue emit_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_binary_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_logical_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_unary_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_cast_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_call_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_method_call_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_array_method_call_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_text_method_call_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_array_literal(const Expression& expression, std::ostream& output);
    TypedValue emit_index_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_slice_expression(const Expression& expression, std::ostream& output);
    TypedValue emit_text_literal(const std::string& lexeme);
    void emit_function(const Statement& statement, std::ostream& output);
    void emit_extern_declarations(std::ostream& output);
    void emit_global_constants(std::ostream& output);
    void emit_print(const TypedValue& value, std::ostream& output);
    void emit_bounds_check(const std::string& index, const std::string& length, std::string_view message,
                           std::ostream& output);
    void emit_slice_bounds_check(const std::string& start, const std::string& end, const std::string& length,
                                 std::ostream& output);
    void collect_functions(const Program& program);
    void collect_function(const Statement& statement);
    void collect_global_constants(const Program& program);

    std::string next_register();
    std::string next_label(std::string_view name);
    std::string llvm_type(const Type& type) const;
    std::size_t llvm_bit_width(const Type& type) const;
    std::string printf_format_name(const Type& type) const;
    std::size_t llvm_size(const Type& type) const;
    std::string function_name(const std::string& name) const;
    std::string extern_function_name(const FunctionSignature& signature) const;
    std::string default_value(const Type& type) const;
    std::string decode_glyph_literal(const std::string& lexeme) const;
    std::string decode_text_literal(const std::string& lexeme) const;
    std::string llvm_text_literal(const std::string& value) const;
    std::string llvm_symbol(const std::string& value) const;
    TypedValue cast_for_print(const TypedValue& value, std::ostream& output);
    TypedValue cast_value(const TypedValue& value, const Type& target, std::ostream& output);
    std::string emit_index_as_i64(const TypedValue& index, std::ostream& output);

    std::unordered_map<const Expression*, Type> expression_types_;
    std::unordered_map<const Expression*, std::string> resolved_calls_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, Local> locals_;
    std::vector<const Statement*> global_constants_;
    std::vector<std::string> string_globals_;
    std::vector<LoopLabels> loop_stack_;
    Type current_return_type_;
    std::size_t register_count_ = 0;
    std::size_t label_count_ = 0;
    std::size_t string_literal_count_ = 0;
};

} // namespace dune
