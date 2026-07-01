#pragma once

#include "ast/ast.hpp"
#include "compiler/bytecode.hpp"
#include "typechecker/type_checker.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dune {

class Compiler {
public:
    Bytecode compile(const Program& program);

private:
    void collect_function(const Statement& statement);
    void collect_functions(const std::vector<Statement>& statements);
    void collect_structs(const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs);
    void collect_enums(const std::unordered_map<std::string, TypeChecker::EnumDefinition>& enums);
    void collect_type_aliases(const std::vector<Statement>& statements);
    void collect_global_constants(const std::vector<Statement>& statements);
    void compile_function(const Statement& statement);
    void compile_global_constants();
    void compile_statements(const std::vector<Statement>& statements);
    void compile_statement(const Statement& statement);
    void compile_for_in_statement(const Statement& statement);
    void compile_range_for_in_statement(const Statement& statement, const Type& element_type);
    void compile_array_for_in_statement(const Statement& statement, const Type& iterable_type);
    void compile_array_comprehension(const Expression& expression);
    void compile_comprehension_body(const Expression& comprehension, std::size_t result_slot,
                                    const Expression* condition);
    void compile_try_expression(const Expression& expression);
    void compile_expression(const Expression& expression);
    void compile_method_call_expression(const Expression& expression);
    void compile_format_expression(const Expression& expression);
    bool compile_io_builtin_expression(const Expression& expression);
    void compile_member_expression(const Expression& expression);
    void compile_when_expression(const Expression& expression);
    void compile_assignment_target(const Expression& target, const Expression& value);
    void compile_tuple_destructuring_assignment(const Expression& target, const Expression& value);
    void compile_variant_constructor(const Expression& expression);
    void compile_tuple_literal(const Expression& expression);
    void compile_struct_literal(const Expression& expression);
    void compile_binary_expression(const Expression& expression);
    void compile_cast_expression(const Expression& expression);
    void compile_slice_expression(const Expression& expression);

    std::size_t add_constant(Value value);
    std::size_t declare_local(const std::string& name, const Type& type);
    std::size_t declare_scoped_local(const std::string& name, const Type& type);
    std::size_t resolve_local(const std::string& name) const;
    const Type& expression_type(const Expression& expression) const;
    std::size_t resolve_function(const std::string& name) const;
    Type normalize_type(const Type& type) const;
    Type normalize_type(const Type& type, std::unordered_set<std::string>& resolving_aliases) const;
    void reset_scopes();
    void push_scope();
    void pop_scope();
    std::size_t emit(OpCode op, std::size_t operand = 0);
    void patch_operand(std::size_t instruction_index, std::size_t operand);

    struct LoopJumps {
        std::vector<std::size_t> breaks;
        std::vector<std::size_t> continues;
    };

    struct StructLayout {
        std::vector<Parameter> fields;
        std::unordered_map<std::string, std::size_t> field_indices;
    };

    struct ScopedLocal {
        std::string name;
        bool had_previous = false;
        std::size_t previous_slot = 0;
        Type previous_type;
    };

    Bytecode bytecode_;
    std::unordered_map<std::string, std::size_t> locals_;
    std::unordered_map<std::string, Type> local_types_;
    std::vector<std::vector<ScopedLocal>> local_scopes_;
    std::unordered_map<std::string, std::size_t> functions_;
    std::unordered_map<std::string, StructLayout> structs_;
    std::unordered_set<std::string> enums_;
    std::unordered_map<std::string, Type> type_aliases_;
    std::vector<const Statement*> global_constants_;
    std::unordered_map<const Expression*, Type> expression_types_;
    std::unordered_map<const Expression*, std::string> resolved_calls_;
    std::unordered_map<const Expression*, TypeChecker::VariantResolution> resolved_variants_;
    std::unordered_map<const Expression*, TypeChecker::TryResolution> resolved_tries_;
    std::vector<LoopJumps> loop_stack_;
    std::vector<Instruction>* instructions_ = nullptr;
    std::size_t temporary_count_ = 0;
    std::size_t local_count_ = 0;
};

} // namespace dune
