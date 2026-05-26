#pragma once

#include "ast/ast.hpp"
#include "compiler/bytecode.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace dune {

class Compiler {
public:
    Bytecode compile(const Program& program);

private:
    void collect_functions(const std::vector<Statement>& statements);
    void compile_function(const Statement& statement);
    void compile_statements(const std::vector<Statement>& statements);
    void compile_statement(const Statement& statement);
    void compile_expression(const Expression& expression);

    std::size_t add_constant(Value value);
    std::size_t declare_local(const std::string& name, const Type& type);
    std::size_t resolve_local(const std::string& name) const;
    const Type& expression_type(const Expression& expression) const;
    std::size_t resolve_function(const std::string& name) const;
    std::size_t emit(OpCode op, std::size_t operand = 0);
    void patch_operand(std::size_t instruction_index, std::size_t operand);

    Bytecode bytecode_;
    std::unordered_map<std::string, std::size_t> locals_;
    std::unordered_map<std::string, Type> local_types_;
    std::unordered_map<std::string, std::size_t> functions_;
    std::unordered_map<const Expression*, Type> expression_types_;
    std::unordered_map<const Expression*, std::string> resolved_calls_;
    std::vector<Instruction>* instructions_ = nullptr;
};

} // namespace dune
