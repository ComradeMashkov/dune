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
    void compile_statements(const std::vector<Statement>& statements);
    void compile_statement(const Statement& statement);
    void compile_expression(const Expression& expression);

    std::size_t add_constant(int value);
    std::size_t declare_local(const std::string& name);
    std::size_t resolve_local(const std::string& name) const;
    std::size_t emit(OpCode op, std::size_t operand = 0);
    void patch_operand(std::size_t instruction_index, std::size_t operand);

    Bytecode bytecode_;
    std::unordered_map<std::string, std::size_t> locals_;
};

} // namespace dune
