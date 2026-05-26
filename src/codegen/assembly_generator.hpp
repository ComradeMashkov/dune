#pragma once

#include "ast/ast.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace dune {

class AssemblyGenerator {
public:
    void generate(const Program& program, std::ostream& output);

private:
    void assign_slots(const Program& program);
    void emit_header(std::ostream& output) const;
    void emit_footer(std::ostream& output) const;
    void emit_statement(const Statement& statement, std::ostream& output);
    void emit_expression(const Expression& expression, std::ostream& output);
    void emit_print(std::ostream& output) const;
    void emit_store(std::size_t slot, std::ostream& output) const;
    void emit_load(std::size_t slot, std::ostream& output) const;
    void emit_number(const std::string& number, std::ostream& output) const;
    void emit_binary(const std::string& op, std::ostream& output) const;

    std::size_t declare_slot(const std::string& name);
    std::size_t resolve_slot(const std::string& name) const;
    std::size_t slot_offset(std::size_t slot) const;

    std::unordered_map<std::string, std::size_t> slots_;
    std::unordered_set<std::string> declared_;
    std::size_t stack_size_ = 0;
};

} // namespace dune
