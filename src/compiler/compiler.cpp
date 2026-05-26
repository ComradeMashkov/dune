#include "compiler.hpp"

#include "typechecker/type_checker.hpp"

#include <cstdint>
#include <stdexcept>
#include <utility>

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

Value make_text(std::string value) {
    Value result;
    result.kind = ValueKind::text;
    result.text_value = std::move(value);
    return result;
}

Value make_unit() {
    Value result;
    result.kind = ValueKind::unit;
    return result;
}

bool is_signed_type(ValueType type) {
    return type == ValueType::int_type || type == ValueType::i8_type || type == ValueType::i16_type ||
           type == ValueType::i32_type || type == ValueType::i64_type || type == ValueType::isize_type;
}

bool is_unsigned_type(ValueType type) {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type || type == ValueType::usize_type;
}

bool is_real_type(ValueType type) {
    return type == ValueType::real32_type || type == ValueType::real_type;
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

std::string decode_text_literal(const std::string& lexeme) {
    std::string result;
    for (std::size_t index = 1; index + 1 < lexeme.size(); ++index) {
        char current = lexeme[index];
        if (current != '\\') {
            result += current;
            continue;
        }

        ++index;
        if (index + 1 >= lexeme.size()) {
            throw std::runtime_error("invalid text literal");
        }

        switch (lexeme[index]) {
        case 'n':
            result += '\n';
            break;
        case 'r':
            result += '\r';
            break;
        case 't':
            result += '\t';
            break;
        case '0':
            result += '\0';
            break;
        case '"':
            result += '"';
            break;
        case '\\':
            result += '\\';
            break;
        default:
            throw std::runtime_error("unknown text escape");
        }
    }

    return result;
}

Value make_number(const std::string& lexeme, const Type& type) {
    if (is_real_type(type.kind)) {
        return make_real(std::stod(lexeme));
    }

    if (is_unsigned_type(type.kind)) {
        return make_unsigned(std::stoull(lexeme));
    }

    return make_signed(std::stoll(lexeme));
}

Value default_value(const Type& type) {
    if (is_unsigned_type(type.kind)) {
        return make_unsigned(0);
    }

    if (is_signed_type(type.kind)) {
        return make_signed(0);
    }

    if (is_real_type(type.kind)) {
        return make_real(0.0);
    }

    if (type.kind == ValueType::bool_type) {
        return make_bool(false);
    }

    if (type.kind == ValueType::glyph_type) {
        return make_glyph('\0');
    }

    if (type.kind == ValueType::text_type) {
        return make_text("");
    }

    return make_unit();
}

} // namespace

Bytecode Compiler::compile(const Program& program) {
    TypeChecker type_checker;
    type_checker.check(program);

    bytecode_ = Bytecode{};
    locals_.clear();
    local_types_.clear();
    functions_.clear();
    global_constants_.clear();
    expression_types_ = type_checker.expression_types();
    resolved_calls_ = type_checker.resolved_calls();
    instructions_ = &bytecode_.instructions;

    collect_global_constants(program.statements);
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
        std::vector<Type> parameters;
        parameters.reserve(statement.parameters.size());
        for (const Parameter& parameter : statement.parameters) {
            parameters.push_back(parameter.type.has_type ? parameter.type.type : make_type(ValueType::int_type));
        }

        functions_.emplace(function_key(statement.name, parameters), index);
        bytecode_.functions.push_back(Bytecode::Function{statement.name, statement.parameters.size(), 0, {}});
    }
}

void Compiler::collect_global_constants(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind == StatementKind::const_statement && statement.name.find('.') != std::string::npos) {
            global_constants_.push_back(&statement);
        }
    }
}

void Compiler::compile_function(const Statement& statement) {
    std::vector<Type> parameters;
    parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameters.push_back(parameter.type.has_type ? parameter.type.type : make_type(ValueType::int_type));
    }

    const std::size_t function_index = resolve_function(function_key(statement.name, parameters));
    Bytecode::Function& function = bytecode_.functions.at(function_index);

    locals_.clear();
    local_types_.clear();
    instructions_ = &function.instructions;
    for (const Parameter& parameter : statement.parameters) {
        declare_local(parameter.name, parameter.type.has_type ? parameter.type.type : make_type(ValueType::int_type));
    }

    compile_global_constants();
    compile_statements(statement.body);
    emit(OpCode::push_constant,
         add_constant(default_value(statement.type.has_type ? statement.type.type : make_type(ValueType::int_type))));
    emit(OpCode::return_value);

    function.local_count = locals_.size();
}

void Compiler::compile_global_constants() {
    for (const Statement* statement : global_constants_) {
        compile_expression(*statement->expression);
        const Type type = statement->type.has_type ? statement->type.type : expression_type(*statement->expression);
        emit(OpCode::store_local, declare_local(statement->name, type));
    }
}

void Compiler::compile_statements(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        compile_statement(statement);
    }
}

void Compiler::compile_statement(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::let:
    case StatementKind::const_statement: {
        compile_expression(*statement.expression);
        const Type type = statement.type.has_type ? statement.type.type : expression_type(*statement.expression);
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
        if (statement.expression == nullptr) {
            emit(OpCode::push_constant, add_constant(make_unit()));
        } else {
            compile_expression(*statement.expression);
        }

        emit(OpCode::return_value);
        return;
    case StatementKind::expression_statement:
        compile_expression(*statement.expression);
        emit(OpCode::pop);
        return;
    case StatementKind::import_statement:
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
    case ExpressionKind::string:
        emit(OpCode::push_constant, add_constant(make_text(decode_text_literal(expression.lexeme))));
        return;
    case ExpressionKind::boolean:
        emit(OpCode::push_constant, add_constant(make_bool(expression.lexeme == "true")));
        return;
    case ExpressionKind::array:
        for (const std::unique_ptr<Expression>& element : expression.arguments) {
            compile_expression(*element);
        }

        emit(OpCode::make_array, expression.arguments.size());
        return;
    case ExpressionKind::index:
        compile_expression(*expression.left);
        compile_expression(*expression.right);
        emit(OpCode::load_index);
        return;
    case ExpressionKind::member:
        if (expression.left->kind == ExpressionKind::identifier) {
            emit(OpCode::load_local, resolve_local(expression.left->lexeme + "." + expression.lexeme));
            return;
        }

        throw std::runtime_error("unknown member '" + expression.lexeme + "'");
    case ExpressionKind::unary:
        compile_expression(*expression.right);
        if (expression.lexeme == "-") {
            emit(OpCode::negate);
            return;
        }

        if (expression.lexeme == "!") {
            emit(OpCode::not_value);
            return;
        }

        throw std::runtime_error("unknown unary operator '" + expression.lexeme + "'");
    case ExpressionKind::cast:
        compile_cast_expression(expression);
        return;
    case ExpressionKind::call:
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            compile_expression(*argument);
        }

        emit(OpCode::call, resolve_function(resolved_calls_.at(&expression)));
        return;
    case ExpressionKind::method_call:
        compile_method_call_expression(expression);
        return;
    case ExpressionKind::binary:
        compile_binary_expression(expression);
        return;
    }
}

void Compiler::compile_method_call_expression(const Expression& expression) {
    if (resolved_calls_.contains(&expression)) {
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            compile_expression(*argument);
        }

        emit(OpCode::call, resolve_function(resolved_calls_.at(&expression)));
        return;
    }

    const Type receiver = expression_type(*expression.left);
    if (receiver.kind == ValueType::array_type) {
        if (expression.lexeme == "len") {
            compile_expression(*expression.left);
            emit(OpCode::array_len);
            return;
        }

        if (expression.lexeme == "push") {
            compile_expression(*expression.left);
            compile_expression(*expression.arguments.at(0));
            emit(OpCode::array_push);
            return;
        }

        if (expression.lexeme == "pop") {
            compile_expression(*expression.left);
            emit(OpCode::array_pop);
            return;
        }

        if (expression.lexeme == "clear") {
            compile_expression(*expression.left);
            emit(OpCode::array_clear);
            return;
        }

        if (expression.lexeme == "is_empty") {
            compile_expression(*expression.left);
            emit(OpCode::array_is_empty);
            return;
        }
    }

    if (receiver.kind == ValueType::text_type) {
        if (expression.lexeme == "len") {
            compile_expression(*expression.left);
            emit(OpCode::text_len);
            return;
        }

        if (expression.lexeme == "is_empty") {
            compile_expression(*expression.left);
            emit(OpCode::text_is_empty);
            return;
        }

        if (expression.lexeme == "contains") {
            compile_expression(*expression.left);
            compile_expression(*expression.arguments.at(0));
            emit(OpCode::text_contains);
            return;
        }

        if (expression.lexeme == "starts_with") {
            compile_expression(*expression.left);
            compile_expression(*expression.arguments.at(0));
            emit(OpCode::text_starts_with);
            return;
        }
    }

    throw std::runtime_error("unknown method '" + expression.lexeme + "'");
}

void Compiler::compile_binary_expression(const Expression& expression) {
    if (expression.lexeme == "&&") {
        compile_expression(*expression.left);
        const std::size_t false_jump = emit(OpCode::jump_if_false);
        compile_expression(*expression.right);
        const std::size_t end_jump = emit(OpCode::jump);
        patch_operand(false_jump, instructions_->size());
        emit(OpCode::push_constant, add_constant(make_bool(false)));
        patch_operand(end_jump, instructions_->size());
        return;
    }

    if (expression.lexeme == "||") {
        compile_expression(*expression.left);
        const std::size_t right_jump = emit(OpCode::jump_if_false);
        emit(OpCode::push_constant, add_constant(make_bool(true)));
        const std::size_t end_jump = emit(OpCode::jump);
        patch_operand(right_jump, instructions_->size());
        compile_expression(*expression.right);
        patch_operand(end_jump, instructions_->size());
        return;
    }

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

    if (expression.lexeme == "%") {
        emit(OpCode::modulo);
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

void Compiler::compile_cast_expression(const Expression& expression) {
    compile_expression(*expression.left);
    const Type target = expression.type.type;
    if (target.kind == ValueType::text_type || target.kind == ValueType::unit_type ||
        target.kind == ValueType::array_type) {
        return;
    }

    if (is_signed_type(target.kind)) {
        emit(OpCode::cast_signed);
        return;
    }

    if (is_unsigned_type(target.kind)) {
        emit(OpCode::cast_unsigned);
        return;
    }

    if (is_real_type(target.kind)) {
        emit(OpCode::cast_real);
        return;
    }

    if (target.kind == ValueType::bool_type) {
        emit(OpCode::cast_bool);
        return;
    }

    if (target.kind == ValueType::glyph_type) {
        emit(OpCode::cast_glyph);
        return;
    }
}

std::size_t Compiler::add_constant(Value value) {
    bytecode_.constants.push_back(std::move(value));
    return bytecode_.constants.size() - 1;
}

std::size_t Compiler::declare_local(const std::string& name, const Type& type) {
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

const Type& Compiler::expression_type(const Expression& expression) const {
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
