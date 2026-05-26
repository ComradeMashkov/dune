#include "compiler.hpp"

#include "typechecker/type_checker.hpp"

#include <stdexcept>

namespace dune {

Bytecode Compiler::compile(const Program& program) {
    TypeChecker type_checker;
    type_checker.check(program);

    bytecode_ = Bytecode{};
    locals_.clear();
    functions_.clear();
    instructions_ = &bytecode_.instructions;

    collect_functions(program.statements);
    compile_statements(program.statements);

    emit(OpCode::halt);
    bytecode_.local_count = locals_.size();

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            compile_function(statement);
        }
    }

    instructions_ = nullptr;
    return bytecode_;
}

void Compiler::collect_functions(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind != StatementKind::function) {
            continue;
        }

        const std::size_t index = bytecode_.functions.size();
        functions_.emplace(statement.name, index);
        bytecode_.functions.push_back(Bytecode::Function{statement.name, statement.parameters.size(), 0, {}});
    }
}

void Compiler::compile_function(const Statement& statement) {
    const std::size_t function_index = resolve_function(statement.name);
    Bytecode::Function& function = bytecode_.functions.at(function_index);

    locals_.clear();
    instructions_ = &function.instructions;
    for (const Parameter& parameter : statement.parameters) {
        declare_local(parameter.name);
    }

    compile_statements(statement.body);
    emit(OpCode::push_constant, add_constant(0));
    emit(OpCode::return_value);

    function.local_count = locals_.size();
}

void Compiler::compile_statements(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        compile_statement(statement);
    }
}

void Compiler::compile_statement(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::let: {
        compile_expression(*statement.expression);
        emit(OpCode::store_local, declare_local(statement.name));
        return;
    }
    case StatementKind::assign:
        compile_expression(*statement.expression);
        emit(OpCode::store_local, resolve_local(statement.name));
        return;
    case StatementKind::print:
        compile_expression(*statement.expression);
        emit(OpCode::print);
        return;
    case StatementKind::block:
        compile_statements(statement.body);
        return;
    case StatementKind::if_statement: {
        compile_expression(*statement.expression);
        const std::size_t false_jump = emit(OpCode::jump_if_false);
        compile_statements(statement.body);

        if (statement.else_body.empty()) {
            patch_operand(false_jump, instructions_->size());
            return;
        }

        const std::size_t end_jump = emit(OpCode::jump);
        patch_operand(false_jump, instructions_->size());
        compile_statements(statement.else_body);
        patch_operand(end_jump, instructions_->size());
        return;
    }
    case StatementKind::while_statement: {
        const std::size_t loop_start = instructions_->size();
        compile_expression(*statement.expression);
        const std::size_t exit_jump = emit(OpCode::jump_if_false);
        compile_statements(statement.body);
        emit(OpCode::jump, loop_start);
        patch_operand(exit_jump, instructions_->size());
        return;
    }
    case StatementKind::function:
        return;
    case StatementKind::return_statement:
        compile_expression(*statement.expression);
        emit(OpCode::return_value);
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
    case ExpressionKind::boolean:
        emit(OpCode::push_constant, add_constant(expression.lexeme == "true" ? 1 : 0));
        return;
    case ExpressionKind::call:
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            compile_expression(*argument);
        }

        emit(OpCode::call, resolve_function(expression.lexeme));
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

        if (expression.lexeme == "==") {
            emit(OpCode::equal);
            return;
        }

        if (expression.lexeme == "!=") {
            emit(OpCode::not_equal);
            return;
        }

        if (expression.lexeme == ">") {
            emit(OpCode::greater);
            return;
        }

        if (expression.lexeme == ">=") {
            emit(OpCode::greater_equal);
            return;
        }

        if (expression.lexeme == "<") {
            emit(OpCode::less);
            return;
        }

        if (expression.lexeme == "<=") {
            emit(OpCode::less_equal);
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

std::size_t Compiler::resolve_function(const std::string& name) const {
    const auto existing = functions_.find(name);
    if (existing == functions_.end()) {
        throw std::runtime_error("undefined function '" + name + "'");
    }

    return existing->second;
}

std::size_t Compiler::emit(OpCode op, std::size_t operand) {
    instructions_->push_back(Instruction{op, operand});
    return instructions_->size() - 1;
}

void Compiler::patch_operand(std::size_t instruction_index, std::size_t operand) {
    instructions_->at(instruction_index).operand = operand;
}

} // namespace dune
