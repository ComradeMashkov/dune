#include "compiler.hpp"

#include "typechecker/type_checker.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
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

Value make_record(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::record;
    result.record_value = std::make_shared<std::vector<Value>>(std::move(values));
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

    if (type.kind == ValueType::struct_type) {
        return make_record({});
    }

    if (type.kind == ValueType::enum_type) {
        Value result;
        result.kind = ValueKind::variant;
        result.variant_tag = 0;
        return result;
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
    structs_.clear();
    enums_.clear();
    global_constants_.clear();
    loop_stack_.clear();
    temporary_count_ = 0;
    expression_types_ = type_checker.expression_types();
    resolved_calls_ = type_checker.resolved_calls();
    resolved_variants_ = type_checker.resolved_variants();
    collect_structs(type_checker.structs());
    collect_enums(type_checker.enums());
    const auto& instantiated_functions = type_checker.instantiated_functions();
    instructions_ = &bytecode_.instructions;
    local_count_ = 0;

    collect_global_constants(program.statements);
    collect_functions(program.statements);
    for (const Statement& statement : instantiated_functions) {
        collect_function(statement);
    }
    compile_statements(program.statements);

    emit(OpCode::halt);
    bytecode_.local_count = local_count_;

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function && statement.generic_parameters.empty()) {
            compile_function(statement);
        }
    }
    for (const Statement& statement : instantiated_functions) {
        compile_function(statement);
    }

    instructions_ = nullptr;
    return bytecode_;
}

void Compiler::collect_functions(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        collect_function(statement);
    }
}

void Compiler::collect_function(const Statement& statement) {
    if (statement.kind != StatementKind::function || !statement.generic_parameters.empty()) {
        return;
    }

    const std::size_t index = bytecode_.functions.size();
    std::vector<Type> parameters;
    parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameters.push_back(parameter.type.has_type ? normalize_type(parameter.type.type)
                                                     : make_type(ValueType::int_type));
    }

    functions_.emplace(function_key(statement.name, parameters), index);
    const std::string extern_symbol = statement.extern_symbol.empty() ? statement.name : statement.extern_symbol;
    bytecode_.functions.push_back(
        Bytecode::Function{statement.name, extern_symbol, statement.parameters.size(), 0, {}, statement.is_extern});
}

void Compiler::collect_structs(const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs) {
    for (const auto& [name, definition] : structs) {
        StructLayout layout;
        for (const TypeChecker::StructField& field : definition.fields) {
            layout.field_indices.emplace(field.name, layout.fields.size());
            layout.fields.push_back(Parameter{field.name, TypeAnnotation{true, field.type}, field.location});
        }

        structs_.emplace(name, std::move(layout));
    }
}

void Compiler::collect_enums(const std::unordered_map<std::string, TypeChecker::EnumDefinition>& enums) {
    for (const auto& [name, definition] : enums) {
        (void)definition;
        enums_.insert(name);
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
        parameters.push_back(parameter.type.has_type ? normalize_type(parameter.type.type)
                                                     : make_type(ValueType::int_type));
    }

    const std::size_t function_index = resolve_function(function_key(statement.name, parameters));
    Bytecode::Function& function = bytecode_.functions.at(function_index);
    if (function.is_extern) {
        return;
    }

    locals_.clear();
    local_types_.clear();
    loop_stack_.clear();
    temporary_count_ = 0;
    local_count_ = 0;
    instructions_ = &function.instructions;
    for (const Parameter& parameter : statement.parameters) {
        declare_local(parameter.name,
                      parameter.type.has_type ? normalize_type(parameter.type.type) : make_type(ValueType::int_type));
    }

    compile_global_constants();
    const Type return_type =
        statement.type.has_type ? normalize_type(statement.type.type) : make_type(ValueType::int_type);
    const bool has_tail_expression =
        !statement.body.empty() && statement.body.back().kind == StatementKind::expression_statement &&
        statement.body.back().expression != nullptr && return_type.kind != ValueType::unit_type;
    if (has_tail_expression) {
        for (std::size_t index = 0; index + 1 < statement.body.size(); ++index) {
            compile_statement(statement.body[index]);
        }
        compile_expression(*statement.body.back().expression);
        emit(OpCode::return_value);
        function.local_count = local_count_;
        return;
    }

    compile_statements(statement.body);
    emit(OpCode::push_constant, add_constant(default_value(return_type)));
    emit(OpCode::return_value);

    function.local_count = local_count_;
}

void Compiler::compile_global_constants() {
    for (const Statement* statement : global_constants_) {
        compile_expression(*statement->expression);
        const Type type =
            statement->type.has_type ? normalize_type(statement->type.type) : expression_type(*statement->expression);
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
    case StatementKind::binding:
    case StatementKind::const_statement: {
        compile_expression(*statement.expression);
        const Type type =
            statement.type.has_type ? normalize_type(statement.type.type) : expression_type(*statement.expression);
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
        loop_stack_.push_back(LoopJumps{});
        compile_statements(statement.body);
        LoopJumps jumps = std::move(loop_stack_.back());
        loop_stack_.pop_back();
        for (const std::size_t jump : jumps.continues) {
            patch_operand(jump, loop_start);
        }
        emit(OpCode::jump, loop_start);
        patch_operand(exit_jump, instructions_->size());
        for (const std::size_t jump : jumps.breaks) {
            patch_operand(jump, instructions_->size());
        }
        return;
    }
    case StatementKind::for_statement: {
        if (statement.initializer != nullptr) {
            compile_statement(*statement.initializer);
        }

        const std::size_t loop_start = instructions_->size();
        compile_expression(*statement.expression);
        const std::size_t exit_jump = emit(OpCode::jump_if_false);
        loop_stack_.push_back(LoopJumps{});
        compile_statements(statement.body);
        LoopJumps jumps = std::move(loop_stack_.back());
        loop_stack_.pop_back();
        const std::size_t continue_target = instructions_->size();
        for (const std::size_t jump : jumps.continues) {
            patch_operand(jump, continue_target);
        }
        if (statement.increment != nullptr) {
            compile_statement(*statement.increment);
        }
        emit(OpCode::jump, loop_start);
        patch_operand(exit_jump, instructions_->size());
        for (const std::size_t jump : jumps.breaks) {
            patch_operand(jump, instructions_->size());
        }
        return;
    }
    case StatementKind::break_statement:
        if (loop_stack_.empty()) {
            throw std::runtime_error("break statement outside loop");
        }
        loop_stack_.back().breaks.push_back(emit(OpCode::jump));
        return;
    case StatementKind::continue_statement:
        if (loop_stack_.empty()) {
            throw std::runtime_error("continue statement outside loop");
        }
        loop_stack_.back().continues.push_back(emit(OpCode::jump));
        return;
    case StatementKind::function:
    case StatementKind::method_block:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
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
    if (resolved_variants_.contains(&expression)) {
        compile_variant_constructor(expression);
        return;
    }

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
    case ExpressionKind::struct_literal:
        compile_struct_literal(expression);
        return;
    case ExpressionKind::index:
        compile_expression(*expression.left);
        compile_expression(*expression.right);
        emit(OpCode::load_index);
        return;
    case ExpressionKind::slice:
        compile_slice_expression(expression);
        return;
    case ExpressionKind::member:
        compile_member_expression(expression);
        return;
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
    case ExpressionKind::when_expression:
        compile_when_expression(expression);
        return;
    }
}

void Compiler::compile_when_expression(const Expression& expression) {
    const Type subject_type = expression_type(*expression.left);
    compile_expression(*expression.left);
    const std::size_t subject_slot =
        declare_local("__when_subject_" + std::to_string(temporary_count_++), subject_type);
    emit(OpCode::store_local, subject_slot);

    if (subject_type.kind == ValueType::enum_type) {
        std::vector<std::size_t> end_jumps;
        for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
            const Expression& pattern = *expression.arguments[index];
            const Expression& result = *expression.arguments[index + 1];
            const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

            std::size_t next_when = 0;
            const TypeChecker::VariantResolution* resolution = nullptr;
            bool had_previous_binding = false;
            std::size_t previous_binding_slot = 0;
            Type previous_binding_type;
            if (!wildcard) {
                resolution = &resolved_variants_.at(&pattern);
                emit(OpCode::load_local, subject_slot);
                emit(OpCode::load_variant_tag);
                emit(OpCode::push_constant, add_constant(make_unsigned(resolution->tag)));
                emit(OpCode::equal);
                next_when = emit(OpCode::jump_if_false);

                if (resolution->binds_payload) {
                    const auto previous_slot = locals_.find(resolution->binding_name);
                    if (previous_slot != locals_.end()) {
                        had_previous_binding = true;
                        previous_binding_slot = previous_slot->second;
                        previous_binding_type = local_types_.at(resolution->binding_name);
                    }

                    emit(OpCode::load_local, subject_slot);
                    emit(OpCode::load_variant_payload);
                    emit(OpCode::store_local, force_local(resolution->binding_name, resolution->payload_type));
                }
            }

            compile_expression(result);
            if (resolution != nullptr && resolution->binds_payload) {
                if (had_previous_binding) {
                    locals_[resolution->binding_name] = previous_binding_slot;
                    local_types_[resolution->binding_name] = previous_binding_type;
                } else {
                    locals_.erase(resolution->binding_name);
                    local_types_.erase(resolution->binding_name);
                }
            }

            end_jumps.push_back(emit(OpCode::jump));
            if (!wildcard) {
                patch_operand(next_when, instructions_->size());
            }
        }

        for (const std::size_t jump : end_jumps) {
            patch_operand(jump, instructions_->size());
        }
        return;
    }

    std::vector<std::size_t> end_jumps;
    for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
        const Expression& pattern = *expression.arguments[index];
        const Expression& result = *expression.arguments[index + 1];
        const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

        std::size_t next_when = 0;
        if (!wildcard) {
            emit(OpCode::load_local, subject_slot);
            compile_expression(pattern);
            emit(OpCode::equal);
            next_when = emit(OpCode::jump_if_false);
        }

        compile_expression(result);
        end_jumps.push_back(emit(OpCode::jump));
        if (!wildcard) {
            patch_operand(next_when, instructions_->size());
        }
    }

    for (const std::size_t jump : end_jumps) {
        patch_operand(jump, instructions_->size());
    }
}

void Compiler::compile_variant_constructor(const Expression& expression) {
    const TypeChecker::VariantResolution& resolution = resolved_variants_.at(&expression);
    if (resolution.has_payload) {
        if (expression.kind == ExpressionKind::call || expression.kind == ExpressionKind::method_call) {
            compile_expression(*expression.arguments.at(0));
        } else {
            throw std::runtime_error("choice variant payload constructor needs an argument");
        }

        emit(OpCode::make_variant, resolution.tag);
        return;
    }

    emit(OpCode::make_unit_variant, resolution.tag);
}

void Compiler::compile_member_expression(const Expression& expression) {
    const auto receiver_type = expression_types_.find(expression.left.get());
    if (receiver_type != expression_types_.end() && receiver_type->second.kind == ValueType::struct_type) {
        const auto layout = structs_.find(receiver_type->second.name);
        if (layout == structs_.end()) {
            throw std::runtime_error("unknown record '" + receiver_type->second.name + "'");
        }

        const auto field = layout->second.field_indices.find(expression.lexeme);
        if (field == layout->second.field_indices.end()) {
            throw std::runtime_error("unknown field '" + expression.lexeme + "'");
        }

        compile_expression(*expression.left);
        emit(OpCode::load_field, field->second);
        return;
    }

    if (expression.left->kind == ExpressionKind::identifier) {
        emit(OpCode::load_local, resolve_local(expression.left->lexeme + "." + expression.lexeme));
        return;
    }

    throw std::runtime_error("unknown member '" + expression.lexeme + "'");
}

void Compiler::compile_struct_literal(const Expression& expression) {
    const auto layout = structs_.find(expression.lexeme);
    if (layout == structs_.end()) {
        throw std::runtime_error("unknown record '" + expression.lexeme + "'");
    }

    for (const Parameter& field : layout->second.fields) {
        const auto source = std::find(expression.field_names.begin(), expression.field_names.end(), field.name);
        if (source == expression.field_names.end()) {
            throw std::runtime_error("missing field '" + field.name + "'");
        }

        const std::size_t index = static_cast<std::size_t>(source - expression.field_names.begin());
        compile_expression(*expression.arguments.at(index));
    }

    emit(OpCode::make_record, layout->second.fields.size());
}

void Compiler::compile_method_call_expression(const Expression& expression) {
    if (resolved_calls_.contains(&expression)) {
        const std::size_t function_index = resolve_function(resolved_calls_.at(&expression));
        const Bytecode::Function& function = bytecode_.functions.at(function_index);
        if (function.arity == expression.arguments.size() + 1) {
            compile_expression(*expression.left);
        } else if (function.arity != expression.arguments.size()) {
            throw std::runtime_error("method call argument count mismatch");
        }

        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            compile_expression(*argument);
        }

        emit(OpCode::call, function_index);
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
    const Type target = normalize_type(expression.type.type);
    if (target.kind == ValueType::text_type || target.kind == ValueType::unit_type ||
        target.kind == ValueType::array_type || target.kind == ValueType::struct_type) {
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

void Compiler::compile_slice_expression(const Expression& expression) {
    compile_expression(*expression.left);
    if (!expression.arguments.empty() && expression.arguments[0] != nullptr) {
        compile_expression(*expression.arguments[0]);
    } else {
        emit(OpCode::push_constant, add_constant(make_signed(-1)));
    }

    if (expression.arguments.size() > 1 && expression.arguments[1] != nullptr) {
        compile_expression(*expression.arguments[1]);
    } else {
        emit(OpCode::push_constant, add_constant(make_signed(-1)));
    }

    emit(OpCode::load_slice);
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

    const std::size_t slot = local_count_++;
    locals_.emplace(name, slot);
    local_types_.emplace(name, type);
    return slot;
}

std::size_t Compiler::force_local(const std::string& name, const Type& type) {
    const std::size_t slot = local_count_++;
    locals_[name] = slot;
    local_types_[name] = type;
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

Type Compiler::normalize_type(const Type& type) const {
    if (type.kind == ValueType::array_type) {
        Type result{ValueType::array_type, nullptr};
        if (type.element != nullptr) {
            result.element = std::make_shared<Type>(normalize_type(*type.element));
        }
        return result;
    }

    std::vector<Type> arguments;
    arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        arguments.push_back(normalize_type(argument));
    }

    if (type.kind == ValueType::generic_type && structs_.contains(type.name)) {
        return make_struct_type(type.name, std::move(arguments));
    }

    if (type.kind == ValueType::generic_type && enums_.contains(type.name)) {
        return make_enum_type(type.name, std::move(arguments));
    }

    if (type.kind == ValueType::enum_type) {
        Type result = type;
        result.arguments = std::move(arguments);
        return result;
    }

    Type result = type;
    result.arguments = std::move(arguments);
    return result;
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
