#include "compiler.hpp"

#include "typechecker/type_checker.hpp"

#include <cstdint>
#include <stdexcept>

namespace dune {

namespace {

Value make_signed(std::int64_t value) {
    Value result;
    result.kind = ValueKind::signed_integer;
    result.signed_value = value;
    return result;
}

Value make_unsigned(std::uint64_t value) {
    Value result;
    result.kind = ValueKind::unsigned_integer;
    result.unsigned_value = value;
    return result;
}

Value make_real(double value) {
    Value result;
    result.kind = ValueKind::real;
    result.real_value = value;
    return result;
}

Value make_bool(bool value) {
    Value result;
    result.kind = ValueKind::boolean;
    result.bool_value = value;
    return result;
}

Value make_glyph(char value) {
    Value result;
    result.kind = ValueKind::glyph;
    result.glyph_value = value;
    return result;
}

bool is_unsigned_type(ValueType type) {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type;
}

char decode_glyph_literal(const std::string& lexeme) {
    if (lexeme.size() == 3) {
        return lexeme[1];
    }

    if (lexeme.size() != 4 || lexeme[1] != '\\') {
        throw std::runtime_error("invalid glyph literal");
    }

    switch (lexeme[2]) {
    case 'n':
        return '\n';
    case 'r':
        return '\r';
    case 't':
        return '\t';
    case '0':
        return '\0';
    case '\'':
        return '\'';
    case '\\':
        return '\\';
    default:
        throw std::runtime_error("unknown glyph escape");
    }
}

Value make_number(const std::string& lexeme, ValueType type) {
    if (type == ValueType::real_type) {
        return make_real(std::stod(lexeme));
    }

    if (is_unsigned_type(type)) {
        return make_unsigned(std::stoull(lexeme));
    }

    return make_signed(std::stoll(lexeme));
}

} // namespace

Bytecode Compiler::compile(const Program& program) {
    TypeChecker type_checker;
    type_checker.check(program);

    bytecode_ = Bytecode{};
    locals_.clear();
    local_types_.clear();
    functions_.clear();
    expression_types_ = type_checker.expression_types();
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
    local_types_.clear();
    instructions_ = &function.instructions;
    for (const Parameter& parameter : statement.parameters) {
        declare_local(parameter.name, parameter.type.has_type ? parameter.type.type : ValueType::int_type);
    }

    compile_statements(statement.body);
    emit(OpCode::push_constant, add_constant(make_signed(0)));
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
        const ValueType type = statement.type.has_type ? statement.type.type : expression_type(*statement.expression);
        emit(OpCode::store_local, declare_local(statement.name, type));
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
        emit(OpCode::push_constant, add_constant(make_number(expression.lexeme, expression_type(expression))));
        return;
    case ExpressionKind::floating:
        emit(OpCode::push_constant, add_constant(make_real(std::stod(expression.lexeme))));
        return;
    case ExpressionKind::character:
        emit(OpCode::push_constant, add_constant(make_glyph(decode_glyph_literal(expression.lexeme))));
        return;
    case ExpressionKind::boolean:
        emit(OpCode::push_constant, add_constant(make_bool(expression.lexeme == "true")));
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

std::size_t Compiler::add_constant(Value value) {
    bytecode_.constants.push_back(value);
    return bytecode_.constants.size() - 1;
}

std::size_t Compiler::declare_local(const std::string& name, ValueType type) {
    const auto existing = locals_.find(name);
    if (existing != locals_.end()) {
        return existing->second;
    }

    const std::size_t slot = locals_.size();
    locals_.emplace(name, slot);
    local_types_.emplace(name, type);
    return slot;
}

std::size_t Compiler::resolve_local(const std::string& name) const {
    const auto existing = locals_.find(name);
    if (existing == locals_.end()) {
        throw std::runtime_error("undefined variable '" + name + "'");
    }

    return existing->second;
}

ValueType Compiler::expression_type(const Expression& expression) const {
    const auto existing = expression_types_.find(&expression);
    if (existing == expression_types_.end()) {
        throw std::runtime_error("missing inferred expression type");
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
