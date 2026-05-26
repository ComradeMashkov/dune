#include "compiler.hpp"

#include <stdexcept>

namespace dune {

Bytecode Compiler::compile(const Program& program) {
    bytecode_ = Bytecode{};
    locals_.clear();

    for (const Statement& statement : program.statements) {
        compile_statement(statement);
    }

    emit(OpCode::halt);
    bytecode_.local_count = locals_.size();
    return bytecode_;
}

void Compiler::compile_statement(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::let: {
        compile_expression(*statement.expression);
        emit(OpCode::store_local, declare_local(statement.name));
        return;
    }
    case StatementKind::print:
        compile_expression(*statement.expression);
        emit(OpCode::print);
        return;
    }
}

void Compiler::compile_expression(const Expression& expression) {
    switch (expression.kind) {
    case ExpressionKind::identifier:
        emit(OpCode::load_local, resolve_local(expression.lexeme));
        return;
    case ExpressionKind::number:
        emit(OpCode::push_constant, add_constant(std::stoi(expression.lexeme)));
        return;
    case ExpressionKind::binary:
        compile_expression(*expression.left);
        compile_expression(*expression.right);

        if (expression.lexeme == "+") {
            emit(OpCode::add);
            return;
        }

        if (expression.lexeme == "-") {
            emit(OpCode::subtract);
            return;
        }

        if (expression.lexeme == "*") {
            emit(OpCode::multiply);
            return;
        }

        if (expression.lexeme == "/") {
            emit(OpCode::divide);
            return;
        }

        throw std::runtime_error("unknown binary operator");
    }
}

std::size_t Compiler::add_constant(int value) {
    bytecode_.constants.push_back(value);
    return bytecode_.constants.size() - 1;
}

std::size_t Compiler::declare_local(const std::string& name) {
    const auto existing = locals_.find(name);
    if (existing != locals_.end()) {
        return existing->second;
    }

    const std::size_t slot = locals_.size();
    locals_.emplace(name, slot);
    return slot;
}

std::size_t Compiler::resolve_local(const std::string& name) const {
    const auto existing = locals_.find(name);
    if (existing == locals_.end()) {
        throw std::runtime_error("undefined variable '" + name + "'");
    }

    return existing->second;
}

void Compiler::emit(OpCode op, std::size_t operand) {
    bytecode_.instructions.push_back(Instruction{op, operand});
}

} // namespace dune
