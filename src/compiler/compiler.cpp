#include "compiler.hpp"

#include "ast/literal_utils.hpp"
#include "typechecker/type_checker.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
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

Type clone_type(const Type& type) {
    Type result{type.kind, nullptr};
    result.name = type.name;
    result.const_value = type.const_value;
    if (type.element != nullptr) {
        result.element = std::make_shared<Type>(clone_type(*type.element));
    }
    result.arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        result.arguments.push_back(clone_type(argument));
    }
    return result;
}

Type substitute_type(const Type& type, const std::unordered_map<std::string, Type>& substitutions) {
    if (type.kind == ValueType::generic_type) {
        const auto replacement = substitutions.find(type.name);
        if (replacement != substitutions.end()) {
            return clone_type(replacement->second);
        }
    }

    Type result = clone_type(type);
    if (result.element != nullptr) {
        result.element = std::make_shared<Type>(substitute_type(*result.element, substitutions));
    }
    for (Type& argument : result.arguments) {
        argument = substitute_type(argument, substitutions);
    }
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
    if (lexeme.size() >= 3 && lexeme[0] == 'r' && lexeme[1] == '"' && lexeme.back() == '"') {
        return lexeme.substr(2, lexeme.size() - 3);
    }

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
        return make_real(std::stod(clean_real_literal(lexeme)));
    }

    const unsigned long long value = parse_unsigned_integer_literal(lexeme);
    if (is_unsigned_type(type.kind)) {
        return make_unsigned(value);
    }

    return make_signed(static_cast<std::int64_t>(value));
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

void expect_same_kind(const Value& left, const Value& right) {
    if (left.kind != right.kind) {
        throw std::runtime_error("foreknown evaluation type mismatch");
    }
}

Value add_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value + right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value + right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value + right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown addition operands");
}

Value subtract_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value - right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value - right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value - right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown subtraction operands");
}

Value multiply_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value * right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value * right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value * right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown multiplication operands");
}

Value divide_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        if (right.signed_value == 0) {
            throw std::runtime_error("division by zero in foreknown expression");
        }
        return make_signed(left.signed_value / right.signed_value);
    case ValueKind::unsigned_integer:
        if (right.unsigned_value == 0) {
            throw std::runtime_error("division by zero in foreknown expression");
        }
        return make_unsigned(left.unsigned_value / right.unsigned_value);
    case ValueKind::real:
        if (right.real_value == 0.0) {
            throw std::runtime_error("division by zero in foreknown expression");
        }
        return make_real(left.real_value / right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown division operands");
}

Value modulo_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        if (right.signed_value == 0) {
            throw std::runtime_error("division by zero in foreknown expression");
        }
        return make_signed(left.signed_value % right.signed_value);
    case ValueKind::unsigned_integer:
        if (right.unsigned_value == 0) {
            throw std::runtime_error("division by zero in foreknown expression");
        }
        return make_unsigned(left.unsigned_value % right.unsigned_value);
    case ValueKind::real:
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown modulo operands");
}

Value negate_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_signed(0 - value.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(0 - value.unsigned_value);
    case ValueKind::real:
        return make_real(0.0 - value.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown unary minus operand");
}

Value not_value(const Value& value) {
    if (value.kind != ValueKind::boolean) {
        throw std::runtime_error("invalid foreknown logical not operand");
    }

    return make_bool(!value.bool_value);
}

bool values_equal(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return left.signed_value == right.signed_value;
    case ValueKind::unsigned_integer:
        return left.unsigned_value == right.unsigned_value;
    case ValueKind::real:
        return left.real_value == right.real_value;
    case ValueKind::boolean:
        return left.bool_value == right.bool_value;
    case ValueKind::glyph:
        return left.glyph_value == right.glyph_value;
    case ValueKind::text:
        return left.text_value == right.text_value;
    case ValueKind::unit:
        return true;
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown equality operands");
}

int compare_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return (left.signed_value > right.signed_value) - (left.signed_value < right.signed_value);
    case ValueKind::unsigned_integer:
        return (left.unsigned_value > right.unsigned_value) - (left.unsigned_value < right.unsigned_value);
    case ValueKind::real:
        return (left.real_value > right.real_value) - (left.real_value < right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown comparison operands");
}

bool bool_value(const Value& value) {
    if (value.kind != ValueKind::boolean) {
        throw std::runtime_error("foreknown condition must be bool");
    }

    return value.bool_value;
}

Value cast_signed_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return value;
    case ValueKind::unsigned_integer:
        return make_signed(static_cast<std::int64_t>(value.unsigned_value));
    case ValueKind::real:
        return make_signed(static_cast<std::int64_t>(value.real_value));
    case ValueKind::boolean:
        return make_signed(value.bool_value ? 1 : 0);
    case ValueKind::glyph:
        return make_signed(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown signed cast operand");
}

Value cast_unsigned_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_unsigned(static_cast<std::uint64_t>(value.signed_value));
    case ValueKind::unsigned_integer:
        return value;
    case ValueKind::real:
        return make_unsigned(static_cast<std::uint64_t>(value.real_value));
    case ValueKind::boolean:
        return make_unsigned(value.bool_value ? 1 : 0);
    case ValueKind::glyph:
        return make_unsigned(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown unsigned cast operand");
}

Value cast_real_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_real(static_cast<double>(value.signed_value));
    case ValueKind::unsigned_integer:
        return make_real(static_cast<double>(value.unsigned_value));
    case ValueKind::real:
        return value;
    case ValueKind::boolean:
        return make_real(value.bool_value ? 1.0 : 0.0);
    case ValueKind::glyph:
        return make_real(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown real cast operand");
}

Value cast_bool_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_bool(value.signed_value != 0);
    case ValueKind::unsigned_integer:
        return make_bool(value.unsigned_value != 0);
    case ValueKind::real:
        return make_bool(value.real_value != 0.0);
    case ValueKind::boolean:
        return value;
    case ValueKind::glyph:
        return make_bool(value.glyph_value != '\0');
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown bool cast operand");
}

Value cast_glyph_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_glyph(static_cast<char>(value.signed_value));
    case ValueKind::unsigned_integer:
        return make_glyph(static_cast<char>(value.unsigned_value));
    case ValueKind::real:
        return make_glyph(static_cast<char>(value.real_value));
    case ValueKind::boolean:
        return make_glyph(static_cast<char>(value.bool_value ? 1 : 0));
    case ValueKind::glyph:
        return value;
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid foreknown glyph cast operand");
}

class ForeknownEvaluator {
public:
    ForeknownEvaluator(const std::unordered_map<std::string, Value>& constants,
                       const std::unordered_map<std::string, const Statement*>& functions,
                       const std::unordered_map<const Expression*, Type>& expression_types,
                       const std::unordered_map<const Expression*, std::string>& resolved_calls)
        : constants_(constants), functions_(functions), expression_types_(expression_types),
          resolved_calls_(resolved_calls) {}

    Value evaluate(const Expression& expression) {
        scopes_.clear();
        scopes_.emplace_back();
        return evaluate_expression(expression);
    }

private:
    struct ReturnSignal {
        Value value;
    };

    struct BreakSignal {};
    struct ContinueSignal {};

    static constexpr std::size_t step_limit_ = 100000;

    void tick() {
        if (++steps_ > step_limit_) {
            throw std::runtime_error("foreknown evaluation exceeded step limit");
        }
    }

    const Type& expression_type(const Expression& expression) const {
        const auto found = expression_types_.find(&expression);
        if (found == expression_types_.end()) {
            throw std::runtime_error("missing foreknown expression type");
        }

        return found->second;
    }

    Value* find_local(const std::string& name) {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end()) {
                return &found->second;
            }
        }

        return nullptr;
    }

    Value load_name(const std::string& name) {
        if (Value* local = find_local(name); local != nullptr) {
            return *local;
        }

        const auto constant = constants_.find(name);
        if (constant != constants_.end()) {
            return constant->second;
        }

        throw std::runtime_error("unknown foreknown value '" + name + "'");
    }

    void store_local(const std::string& name, Value value) {
        if (Value* local = find_local(name); local != nullptr) {
            *local = std::move(value);
            return;
        }

        throw std::runtime_error("unknown foreknown local '" + name + "'");
    }

    Value evaluate_expression(const Expression& expression) {
        tick();
        switch (expression.kind) {
        case ExpressionKind::identifier:
            return load_name(expression.lexeme);
        case ExpressionKind::number:
            return make_number(expression.lexeme, expression_type(expression));
        case ExpressionKind::floating:
            return make_real(std::stod(clean_real_literal(expression.lexeme)));
        case ExpressionKind::character:
            return make_glyph(decode_glyph_literal(expression.lexeme));
        case ExpressionKind::string:
            return make_text(decode_text_literal(expression.lexeme));
        case ExpressionKind::boolean:
            return make_bool(expression.lexeme == "true");
        case ExpressionKind::member:
            if (expression.left != nullptr && expression.left->kind == ExpressionKind::identifier) {
                return load_name(expression.left->lexeme + "." + expression.lexeme);
            }
            break;
        case ExpressionKind::unary: {
            const Value value = evaluate_expression(*expression.right);
            if (expression.lexeme == "-") {
                return negate_value(value);
            }
            if (expression.lexeme == "!") {
                return not_value(value);
            }
            break;
        }
        case ExpressionKind::cast:
            return cast_value(evaluate_expression(*expression.left), expression.type.type);
        case ExpressionKind::binary:
            return evaluate_binary(expression);
        case ExpressionKind::when_expression:
            return evaluate_when(expression);
        case ExpressionKind::call:
        case ExpressionKind::method_call:
            return evaluate_call(expression);
        case ExpressionKind::array:
        case ExpressionKind::array_comprehension:
        case ExpressionKind::tuple:
        case ExpressionKind::struct_literal:
        case ExpressionKind::index:
        case ExpressionKind::slice:
        case ExpressionKind::try_expression:
        case ExpressionKind::range:
            break;
        }

        throw std::runtime_error("unsupported foreknown expression");
    }

    Value cast_value(Value value, const Type& target) {
        if (is_signed_type(target.kind)) {
            return cast_signed_value(value);
        }
        if (is_unsigned_type(target.kind)) {
            return cast_unsigned_value(value);
        }
        if (is_real_type(target.kind)) {
            return cast_real_value(value);
        }
        if (target.kind == ValueType::bool_type) {
            return cast_bool_value(value);
        }
        if (target.kind == ValueType::glyph_type) {
            return cast_glyph_value(value);
        }
        if (target.kind == ValueType::text_type) {
            return value;
        }

        throw std::runtime_error("unsupported foreknown cast");
    }

    Value evaluate_binary(const Expression& expression) {
        if (expression.lexeme == "&&") {
            const bool left = bool_value(evaluate_expression(*expression.left));
            return make_bool(left && bool_value(evaluate_expression(*expression.right)));
        }

        if (expression.lexeme == "||") {
            const bool left = bool_value(evaluate_expression(*expression.left));
            return make_bool(left || bool_value(evaluate_expression(*expression.right)));
        }

        if (expression.lexeme == "in") {
            throw std::runtime_error("operator 'in' is not supported in foreknown expressions");
        }

        const Value left = evaluate_expression(*expression.left);
        const Value right = evaluate_expression(*expression.right);
        if (expression.lexeme == "+") {
            return add_values(left, right);
        }
        if (expression.lexeme == "-") {
            return subtract_values(left, right);
        }
        if (expression.lexeme == "*") {
            return multiply_values(left, right);
        }
        if (expression.lexeme == "/") {
            return divide_values(left, right);
        }
        if (expression.lexeme == "%") {
            return modulo_values(left, right);
        }
        if (expression.lexeme == "==") {
            return make_bool(values_equal(left, right));
        }
        if (expression.lexeme == "!=") {
            return make_bool(!values_equal(left, right));
        }
        if (expression.lexeme == ">") {
            return make_bool(compare_values(left, right) > 0);
        }
        if (expression.lexeme == ">=") {
            return make_bool(compare_values(left, right) >= 0);
        }
        if (expression.lexeme == "<") {
            return make_bool(compare_values(left, right) < 0);
        }
        if (expression.lexeme == "<=") {
            return make_bool(compare_values(left, right) <= 0);
        }

        throw std::runtime_error("unknown foreknown binary operator");
    }

    Value evaluate_when(const Expression& expression) {
        const Value subject = evaluate_expression(*expression.left);
        for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
            const Expression& pattern = *expression.arguments[index];
            const Expression& result = *expression.arguments[index + 1];
            const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";
            if (wildcard || values_equal(subject, evaluate_expression(pattern))) {
                return evaluate_expression(result);
            }
        }

        throw std::runtime_error("foreknown when expression did not match");
    }

    Value evaluate_call(const Expression& expression) {
        const auto resolved = resolved_calls_.find(&expression);
        if (resolved == resolved_calls_.end()) {
            throw std::runtime_error("foreknown call was not resolved");
        }

        std::vector<Value> arguments;
        arguments.reserve(expression.arguments.size());
        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            arguments.push_back(evaluate_expression(*argument));
        }

        return call_function(resolved->second, std::move(arguments));
    }

    Value call_function(const std::string& key, std::vector<Value> arguments) {
        const auto function = functions_.find(key);
        if (function == functions_.end()) {
            throw std::runtime_error("function is not foreknown");
        }

        const Statement& statement = *function->second;
        if (arguments.size() != statement.parameters.size()) {
            throw std::runtime_error("foreknown function argument count mismatch");
        }

        std::vector<std::unordered_map<std::string, Value>> saved_scopes = std::move(scopes_);
        scopes_.clear();
        scopes_.emplace_back();
        for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
            scopes_.back().emplace(statement.parameters[index].name, std::move(arguments[index]));
        }

        try {
            const Type return_type = statement.type.has_type ? statement.type.type : make_type(ValueType::int_type);
            const bool has_tail_expression =
                !statement.body.empty() && statement.body.back().kind == StatementKind::expression_statement &&
                statement.body.back().expression != nullptr && return_type.kind != ValueType::unit_type;
            if (has_tail_expression) {
                for (std::size_t index = 0; index + 1 < statement.body.size(); ++index) {
                    execute_statement(statement.body[index]);
                }
                Value result = evaluate_expression(*statement.body.back().expression);
                scopes_ = std::move(saved_scopes);
                return result;
            }

            execute_statements(statement.body);
            Value result = default_value(return_type);
            scopes_ = std::move(saved_scopes);
            return result;
        } catch (const ReturnSignal& signal) {
            Value result = signal.value;
            scopes_ = std::move(saved_scopes);
            return result;
        }
    }

    void execute_statements(const std::vector<Statement>& statements) {
        for (const Statement& statement : statements) {
            execute_statement(statement);
        }
    }

    void execute_scoped_statements(const std::vector<Statement>& statements) {
        scopes_.emplace_back();
        try {
            execute_statements(statements);
        } catch (...) {
            scopes_.pop_back();
            throw;
        }
        scopes_.pop_back();
    }

    void execute_statement(const Statement& statement) {
        tick();
        switch (statement.kind) {
        case StatementKind::binding:
        case StatementKind::const_statement:
            scopes_.back().emplace(statement.name, evaluate_expression(*statement.expression));
            return;
        case StatementKind::assign:
            if (statement.target == nullptr || statement.target->kind != ExpressionKind::identifier) {
                throw std::runtime_error("unsupported foreknown assignment target");
            }
            store_local(statement.target->lexeme, evaluate_expression(*statement.expression));
            return;
        case StatementKind::block:
            execute_scoped_statements(statement.body);
            return;
        case StatementKind::if_statement:
            if (bool_value(evaluate_expression(*statement.expression))) {
                execute_scoped_statements(statement.body);
            } else {
                execute_scoped_statements(statement.else_body);
            }
            return;
        case StatementKind::while_statement:
            while (bool_value(evaluate_expression(*statement.expression))) {
                try {
                    execute_scoped_statements(statement.body);
                } catch (const ContinueSignal&) {
                    continue;
                } catch (const BreakSignal&) {
                    break;
                }
            }
            return;
        case StatementKind::for_statement:
            execute_for_statement(statement);
            return;
        case StatementKind::break_statement:
            throw BreakSignal{};
        case StatementKind::continue_statement:
            throw ContinueSignal{};
        case StatementKind::return_statement:
            throw ReturnSignal{statement.expression == nullptr ? make_unit()
                                                               : evaluate_expression(*statement.expression)};
        case StatementKind::expression_statement:
            evaluate_expression(*statement.expression);
            return;
        case StatementKind::for_in_statement:
        case StatementKind::function:
        case StatementKind::method_block:
        case StatementKind::struct_statement:
        case StatementKind::enum_statement:
        case StatementKind::contract_statement:
        case StatementKind::type_alias_statement:
        case StatementKind::import_statement:
        case StatementKind::module_declaration:
        case StatementKind::test_block:
            break;
        }

        throw std::runtime_error("unsupported foreknown statement");
    }

    void execute_for_statement(const Statement& statement) {
        scopes_.emplace_back();
        try {
            if (statement.initializer != nullptr) {
                execute_statement(*statement.initializer);
            }

            while (statement.expression == nullptr || bool_value(evaluate_expression(*statement.expression))) {
                try {
                    execute_scoped_statements(statement.body);
                } catch (const ContinueSignal&) {
                } catch (const BreakSignal&) {
                    break;
                }

                if (statement.increment != nullptr) {
                    execute_statement(*statement.increment);
                }
            }
        } catch (...) {
            scopes_.pop_back();
            throw;
        }
        scopes_.pop_back();
    }

    const std::unordered_map<std::string, Value>& constants_;
    const std::unordered_map<std::string, const Statement*>& functions_;
    const std::unordered_map<const Expression*, Type>& expression_types_;
    const std::unordered_map<const Expression*, std::string>& resolved_calls_;
    std::vector<std::unordered_map<std::string, Value>> scopes_;
    std::size_t steps_ = 0;
};

} // namespace

Bytecode Compiler::compile(const Program& program) {
    TypeChecker type_checker;
    type_checker.check(program);

    bytecode_ = Bytecode{};
    locals_.clear();
    local_types_.clear();
    local_scopes_.clear();
    functions_.clear();
    foreknown_functions_.clear();
    foreknown_values_.clear();
    structs_.clear();
    enums_.clear();
    type_aliases_.clear();
    global_constants_.clear();
    loop_stack_.clear();
    temporary_count_ = 0;
    expression_types_ = type_checker.expression_types();
    iterable_element_types_ = type_checker.iterable_element_types();
    resolved_calls_ = type_checker.resolved_calls();
    resolved_variants_ = type_checker.resolved_variants();
    resolved_tries_ = type_checker.resolved_tries();
    collect_structs(type_checker.structs());
    collect_enums(type_checker.enums());
    collect_type_aliases(program.statements);
    const auto& instantiated_functions = type_checker.instantiated_functions();
    instructions_ = &bytecode_.instructions;
    local_count_ = 0;
    reset_scopes();

    collect_global_constants(program.statements);
    collect_functions(program.statements);
    for (const Statement& statement : instantiated_functions) {
        collect_function(statement);
    }
    evaluate_foreknown_constants();
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
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::test_block) {
            compile_test(statement);
        }
    }

    instructions_ = nullptr;
    return bytecode_;
}

void Compiler::compile_test(const Statement& statement) {
    const std::size_t function_index = bytecode_.functions.size();
    bytecode_.functions.push_back(
        Bytecode::Function{"__test_" + std::to_string(function_index), "", 0, 0, {}, false});
    bytecode_.tests.push_back(Bytecode::Test{statement.name, function_index});

    Bytecode::Function& function = bytecode_.functions.at(function_index);
    locals_.clear();
    local_types_.clear();
    local_scopes_.clear();
    loop_stack_.clear();
    temporary_count_ = 0;
    local_count_ = 0;
    instructions_ = &function.instructions;
    reset_scopes();
    compile_global_constants();
    compile_statements(statement.body);
    emit(OpCode::push_constant, add_constant(default_value(make_type(ValueType::unit_type))));
    emit(OpCode::return_value);
    function.local_count = local_count_;
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

    const std::string key = function_key(statement.name, parameters);
    functions_.emplace(key, index);
    if (statement.is_foreknown) {
        foreknown_functions_[key] = &statement;
    }
    const std::string extern_symbol = statement.extern_symbol.empty() ? statement.name : statement.extern_symbol;
    bytecode_.functions.push_back(
        Bytecode::Function{statement.name, extern_symbol, statement.parameters.size(), 0, {}, statement.is_extern});
}

void Compiler::collect_structs(const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs) {
    for (const auto& [name, definition] : structs) {
        StructLayout layout;
        for (const TypeChecker::StructField& field : definition.fields) {
            layout.field_indices.emplace(field.name, layout.fields.size());
            layout.fields.push_back(Parameter{field.name, TypeAnnotation{true, field.type}, field.location,
                                              field.exported, field.default_value});
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

void Compiler::collect_type_aliases(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind == StatementKind::type_alias_statement && statement.type.has_type) {
            type_aliases_[statement.name] = TypeAlias{statement.generic_parameters, statement.type.type};
        }
    }
}

void Compiler::collect_global_constants(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind == StatementKind::const_statement) {
            global_constants_.push_back(&statement);
        }
    }
}

void Compiler::evaluate_foreknown_constants() {
    for (const Statement* statement : global_constants_) {
        if (!statement->is_foreknown) {
            continue;
        }

        ForeknownEvaluator evaluator(foreknown_values_, foreknown_functions_, expression_types_, resolved_calls_);
        foreknown_values_[statement->name] = evaluator.evaluate(*statement->expression);
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
    local_scopes_.clear();
    loop_stack_.clear();
    temporary_count_ = 0;
    local_count_ = 0;
    instructions_ = &function.instructions;
    reset_scopes();
    std::vector<std::tuple<std::string, Type, std::size_t>> parameter_locals;
    parameter_locals.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameter_locals.emplace_back(parameter.name,
                                      parameter.type.has_type ? normalize_type(parameter.type.type)
                                                              : make_type(ValueType::int_type),
                                      local_count_++);
    }

    compile_global_constants();
    for (const auto& [name, type, slot] : parameter_locals) {
        ScopedLocal local;
        local.name = name;
        local_scopes_.back().push_back(std::move(local));
        locals_[name] = slot;
        local_types_[name] = type;
    }

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
        if (statement->is_foreknown) {
            emit(OpCode::push_constant, add_constant(foreknown_values_.at(statement->name)));
        } else {
            compile_expression(*statement->expression);
        }
        const Type type =
            statement->type.has_type ? normalize_type(statement->type.type) : expression_type(*statement->expression);
        emit(OpCode::store_local, declare_scoped_local(statement->name, type));
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
        if (statement.is_foreknown) {
            emit(OpCode::push_constant, add_constant(foreknown_values_.at(statement.name)));
        } else {
            compile_expression(*statement.expression);
        }
        const Type type =
            statement.type.has_type ? normalize_type(statement.type.type) : expression_type(*statement.expression);
        emit(OpCode::store_local, declare_scoped_local(statement.name, type));
        return;
    }
    case StatementKind::assign: {
        if (statement.target != nullptr && statement.target->kind == ExpressionKind::tuple) {
            compile_tuple_destructuring_assignment(*statement.target, *statement.expression);
            return;
        }

        if (statement.target != nullptr && statement.target->kind != ExpressionKind::identifier) {
            compile_assignment_target(*statement.target, *statement.expression);
            return;
        }

        std::string name = statement.name;
        if (statement.target != nullptr) {
            name = statement.target->lexeme;
        }
        compile_expression(*statement.expression);
        if (const auto local = locals_.find(name); local != locals_.end()) {
            emit(OpCode::store_local, local->second);
        } else {
            emit(OpCode::store_local, declare_scoped_local(name, expression_type(*statement.expression)));
        }
        return;
    }
    case StatementKind::block:
        push_scope();
        compile_statements(statement.body);
        pop_scope();
        return;
    case StatementKind::if_statement: {
        compile_expression(*statement.expression);
        const std::size_t false_jump = emit(OpCode::jump_if_false);
        push_scope();
        compile_statements(statement.body);
        pop_scope();

        if (statement.else_body.empty()) {
            patch_operand(false_jump, instructions_->size());
            return;
        }

        const std::size_t end_jump = emit(OpCode::jump);
        patch_operand(false_jump, instructions_->size());
        push_scope();
        compile_statements(statement.else_body);
        pop_scope();
        patch_operand(end_jump, instructions_->size());
        return;
    }
    case StatementKind::while_statement: {
        const std::size_t loop_start = instructions_->size();
        compile_expression(*statement.expression);
        const std::size_t exit_jump = emit(OpCode::jump_if_false);
        loop_stack_.push_back(LoopJumps{});
        push_scope();
        compile_statements(statement.body);
        pop_scope();
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
        push_scope();
        if (statement.initializer != nullptr) {
            compile_statement(*statement.initializer);
        }

        const std::size_t loop_start = instructions_->size();
        compile_expression(*statement.expression);
        const std::size_t exit_jump = emit(OpCode::jump_if_false);
        loop_stack_.push_back(LoopJumps{});
        push_scope();
        compile_statements(statement.body);
        pop_scope();
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
        pop_scope();
        return;
    }
    case StatementKind::for_in_statement:
        compile_for_in_statement(statement);
        return;
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
    case StatementKind::contract_statement:
    case StatementKind::type_alias_statement:
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
    case StatementKind::module_declaration:
    // Test bodies are compiled into their own chunks by compile_test(), never
    // into the main instruction stream, so they do not run during `dune run`.
    case StatementKind::test_block:
        return;
    }
}

void Compiler::compile_for_in_statement(const Statement& statement) {
    push_scope();
    const Type iterable_type = normalize_type(expression_type(*statement.expression));
    const Type element_type = iterable_element_type(*statement.expression);
    if (statement.expression->kind == ExpressionKind::range) {
        compile_range_for_in_statement(statement, element_type);
    } else {
        compile_array_for_in_statement(statement, iterable_type, element_type);
    }
    pop_scope();
}

void Compiler::compile_range_for_in_statement(const Statement& statement, const Type& element_type) {
    const Expression& range = *statement.expression;
    compile_expression(*range.left);
    const std::size_t current_slot =
        declare_scoped_local("__for_current_" + std::to_string(temporary_count_++), element_type);
    emit(OpCode::store_local, current_slot);

    compile_expression(*range.right);
    const std::size_t end_slot = declare_scoped_local("__for_end_" + std::to_string(temporary_count_++), element_type);
    emit(OpCode::store_local, end_slot);

    const std::size_t item_slot = declare_scoped_local(statement.name, element_type);
    const std::size_t loop_start = instructions_->size();
    emit(OpCode::load_local, current_slot);
    emit(OpCode::load_local, end_slot);
    emit(OpCode::less);
    const std::size_t loop_exit = emit(OpCode::jump_if_false);

    emit(OpCode::load_local, current_slot);
    emit(OpCode::store_local, item_slot);

    loop_stack_.push_back(LoopJumps{});
    push_scope();
    compile_statements(statement.body);
    pop_scope();
    LoopJumps jumps = std::move(loop_stack_.back());
    loop_stack_.pop_back();
    const std::size_t continue_target = instructions_->size();
    for (const std::size_t jump : jumps.continues) {
        patch_operand(jump, continue_target);
    }
    emit(OpCode::load_local, current_slot);
    emit(OpCode::push_constant, add_constant(make_number("1", element_type)));
    emit(OpCode::add);
    emit(OpCode::store_local, current_slot);
    emit(OpCode::jump, loop_start);
    patch_operand(loop_exit, instructions_->size());
    for (const std::size_t jump : jumps.breaks) {
        patch_operand(jump, instructions_->size());
    }
}

void Compiler::compile_array_for_in_statement(const Statement& statement, const Type& iterable_type,
                                              const Type& element_type) {
    compile_iterable_storage(*statement.expression, iterable_type);
    const Type storage_type = iterable_storage_type(iterable_type, element_type);
    const std::size_t iterable_slot =
        declare_scoped_local("__for_iterable_" + std::to_string(temporary_count_++), storage_type);
    emit(OpCode::store_local, iterable_slot);

    const Type index_type = make_type(ValueType::int_type);
    emit(OpCode::push_constant, add_constant(make_signed(0)));
    const std::size_t index_slot =
        declare_scoped_local("__for_index_" + std::to_string(temporary_count_++), index_type);
    emit(OpCode::store_local, index_slot);

    const std::size_t item_slot = declare_scoped_local(statement.name, element_type);
    const std::size_t loop_start = instructions_->size();
    emit(OpCode::load_local, index_slot);
    emit(OpCode::load_local, iterable_slot);
    emit(OpCode::array_len);
    emit(OpCode::less);
    const std::size_t exit_jump = emit(OpCode::jump_if_false);

    emit(OpCode::load_local, iterable_slot);
    emit(OpCode::load_local, index_slot);
    emit(OpCode::load_index);
    emit(OpCode::store_local, item_slot);

    loop_stack_.push_back(LoopJumps{});
    push_scope();
    compile_statements(statement.body);
    pop_scope();
    LoopJumps jumps = std::move(loop_stack_.back());
    loop_stack_.pop_back();
    const std::size_t continue_target = instructions_->size();
    for (const std::size_t jump : jumps.continues) {
        patch_operand(jump, continue_target);
    }
    emit(OpCode::load_local, index_slot);
    emit(OpCode::push_constant, add_constant(make_signed(1)));
    emit(OpCode::add);
    emit(OpCode::store_local, index_slot);
    emit(OpCode::jump, loop_start);
    patch_operand(exit_jump, instructions_->size());
    for (const std::size_t jump : jumps.breaks) {
        patch_operand(jump, instructions_->size());
    }
}

void Compiler::compile_iterable_storage(const Expression& expression, const Type& iterable_type) {
    if (iterable_type.kind == ValueType::array_type && iterable_type.element != nullptr) {
        compile_expression(expression);
        return;
    }

    std::string field_name;
    if (!known_iterable_record_field(iterable_type, field_name)) {
        throw std::runtime_error("for-in used with non-array type");
    }

    const auto layout = structs_.find(iterable_type.name);
    if (layout == structs_.end()) {
        throw std::runtime_error("unknown record '" + iterable_type.name + "'");
    }

    const auto field = layout->second.field_indices.find(field_name);
    if (field == layout->second.field_indices.end()) {
        throw std::runtime_error("iterable record '" + iterable_type.name + "' is missing backing field '" +
                                 field_name + "'");
    }

    compile_expression(expression);
    emit(OpCode::load_field, field->second);
}

Type Compiler::iterable_storage_type(const Type& iterable_type, const Type& element_type) const {
    if (iterable_type.kind == ValueType::array_type && iterable_type.element != nullptr) {
        return iterable_type;
    }

    std::string field_name;
    if (known_iterable_record_field(iterable_type, field_name)) {
        return make_array_type(element_type);
    }

    throw std::runtime_error("for-in used with non-array type");
}

bool Compiler::known_iterable_record_field(const Type& iterable_type, std::string& field_name) const {
    if (iterable_type.kind != ValueType::struct_type) {
        return false;
    }

    if (iterable_type.name == "set.Set") {
        field_name = "items";
        return true;
    }

    if (iterable_type.name == "matrix.Vector" && iterable_type.arguments.size() == 1) {
        field_name = "data";
        return true;
    }

    return false;
}

void Compiler::compile_comprehension_body(const Expression& comprehension, std::size_t result_slot,
                                          const Expression* condition) {
    std::size_t skip_jump = 0;
    const bool has_condition = condition != nullptr;
    if (has_condition) {
        compile_expression(*condition);
        skip_jump = emit(OpCode::jump_if_false);
    }

    emit(OpCode::load_local, result_slot);
    compile_expression(*comprehension.left);
    emit(OpCode::array_push);
    emit(OpCode::pop);

    if (has_condition) {
        patch_operand(skip_jump, instructions_->size());
    }
}

void Compiler::compile_array_comprehension(const Expression& expression) {
    const Type result_type = expression_type(expression);
    const Expression& iterable = *expression.right;
    const Type iterable_type = normalize_type(expression_type(iterable));
    const Type element_type = iterable_element_type(iterable);
    const Expression* condition = expression.arguments.empty() ? nullptr : expression.arguments.front().get();

    push_scope();

    emit(OpCode::make_array, 0);
    const std::size_t result_slot =
        declare_scoped_local("__comp_result_" + std::to_string(temporary_count_++), result_type);
    emit(OpCode::store_local, result_slot);

    if (iterable.kind == ExpressionKind::range) {
        compile_expression(*iterable.left);
        const std::size_t current_slot =
            declare_scoped_local("__comp_current_" + std::to_string(temporary_count_++), element_type);
        emit(OpCode::store_local, current_slot);

        compile_expression(*iterable.right);
        const std::size_t end_slot =
            declare_scoped_local("__comp_end_" + std::to_string(temporary_count_++), element_type);
        emit(OpCode::store_local, end_slot);

        const std::size_t item_slot = declare_scoped_local(expression.lexeme, element_type);
        const std::size_t loop_start = instructions_->size();
        emit(OpCode::load_local, current_slot);
        emit(OpCode::load_local, end_slot);
        emit(OpCode::less);
        const std::size_t loop_exit = emit(OpCode::jump_if_false);

        emit(OpCode::load_local, current_slot);
        emit(OpCode::store_local, item_slot);

        compile_comprehension_body(expression, result_slot, condition);

        emit(OpCode::load_local, current_slot);
        emit(OpCode::push_constant, add_constant(make_number("1", element_type)));
        emit(OpCode::add);
        emit(OpCode::store_local, current_slot);
        emit(OpCode::jump, loop_start);
        patch_operand(loop_exit, instructions_->size());
    } else {
        compile_iterable_storage(iterable, iterable_type);
        const Type storage_type = iterable_storage_type(iterable_type, element_type);
        const std::size_t iterable_slot =
            declare_scoped_local("__comp_iterable_" + std::to_string(temporary_count_++), storage_type);
        emit(OpCode::store_local, iterable_slot);

        const Type index_type = make_type(ValueType::int_type);
        emit(OpCode::push_constant, add_constant(make_signed(0)));
        const std::size_t index_slot =
            declare_scoped_local("__comp_index_" + std::to_string(temporary_count_++), index_type);
        emit(OpCode::store_local, index_slot);

        const std::size_t item_slot = declare_scoped_local(expression.lexeme, element_type);
        const std::size_t loop_start = instructions_->size();
        emit(OpCode::load_local, index_slot);
        emit(OpCode::load_local, iterable_slot);
        emit(OpCode::array_len);
        emit(OpCode::less);
        const std::size_t exit_jump = emit(OpCode::jump_if_false);

        emit(OpCode::load_local, iterable_slot);
        emit(OpCode::load_local, index_slot);
        emit(OpCode::load_index);
        emit(OpCode::store_local, item_slot);

        compile_comprehension_body(expression, result_slot, condition);

        emit(OpCode::load_local, index_slot);
        emit(OpCode::push_constant, add_constant(make_signed(1)));
        emit(OpCode::add);
        emit(OpCode::store_local, index_slot);
        emit(OpCode::jump, loop_start);
        patch_operand(exit_jump, instructions_->size());
    }

    emit(OpCode::load_local, result_slot);
    pop_scope();
}

void Compiler::compile_try_expression(const Expression& expression) {
    const TypeChecker::TryResolution& resolution = resolved_tries_.at(&expression);
    const Type outcome_type = expression_type(*expression.left);

    // Evaluate the Outcome operand once and stash it in a temporary.
    compile_expression(*expression.left);
    const std::size_t outcome_slot =
        declare_scoped_local("__try_outcome_" + std::to_string(temporary_count_++), outcome_type);
    emit(OpCode::store_local, outcome_slot);

    // If it is the Failed variant, return it from the enclosing function unchanged.
    emit(OpCode::load_local, outcome_slot);
    emit(OpCode::load_variant_tag);
    emit(OpCode::push_constant, add_constant(make_unsigned(resolution.failed_tag)));
    emit(OpCode::equal);
    const std::size_t ok_jump = emit(OpCode::jump_if_false);

    emit(OpCode::load_local, outcome_slot);
    emit(OpCode::return_value);

    // Otherwise unwrap the Done payload as the value of the expression.
    patch_operand(ok_jump, instructions_->size());
    emit(OpCode::load_local, outcome_slot);
    emit(OpCode::load_variant_payload);
}

void Compiler::compile_expression(const Expression& expression) {
    if (resolved_variants_.contains(&expression)) {
        compile_variant_constructor(expression);
        return;
    }

    switch (expression.kind) {
    case ExpressionKind::identifier:
        // A bare identifier that resolved to a function is a function reference:
        // push a callable value rather than loading a local slot.
        if (resolved_calls_.contains(&expression)) {
            emit(OpCode::push_function, resolve_function(resolved_calls_.at(&expression)));
            return;
        }

        emit(OpCode::load_local, resolve_local(expression.lexeme));
        return;
    case ExpressionKind::number:
        emit(OpCode::push_constant, add_constant(make_number(expression.lexeme, expression_type(expression))));
        return;
    case ExpressionKind::floating:
        emit(OpCode::push_constant, add_constant(make_real(std::stod(clean_real_literal(expression.lexeme)))));
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
    case ExpressionKind::array_comprehension:
        compile_array_comprehension(expression);
        return;
    case ExpressionKind::tuple:
        compile_tuple_literal(expression);
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
    case ExpressionKind::try_expression:
        compile_try_expression(expression);
        return;
    case ExpressionKind::cast:
        compile_cast_expression(expression);
        return;
    case ExpressionKind::call:
        if (compile_io_builtin_expression(expression)) {
            return;
        }

        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            compile_expression(*argument);
        }

        // A call the type checker did not bind to a concrete function is a call
        // through a function value: load the callable and dispatch dynamically.
        if (!resolved_calls_.contains(&expression)) {
            emit(OpCode::load_local, resolve_local(expression.lexeme));
            emit(OpCode::call_value);
            return;
        }

        emit(OpCode::call, resolve_function(resolved_calls_.at(&expression)));
        return;
    case ExpressionKind::method_call:
        compile_method_call_expression(expression);
        return;
    case ExpressionKind::binary:
        compile_binary_expression(expression);
        return;
    case ExpressionKind::range:
        throw std::runtime_error("range expressions can only be compiled as for-in iterables");
    case ExpressionKind::when_expression:
        compile_when_expression(expression);
        return;
    }
}

void Compiler::compile_format_expression(const Expression& expression) {
    for (std::size_t index = 0; index < expression.arguments.size(); ++index) {
        // arguments[0] is the format string; the rest are the values to render.
        if (index == 0) {
            compile_expression(*expression.arguments[index]);
        } else {
            compile_printable_argument(*expression.arguments[index]);
        }
    }

    emit(OpCode::format_text, expression.arguments.size() - 1);
}

// Compile a value for `fmt.format`. A record that implements the Display
// contract (a `to_text(): text` method) is lowered to a `to_text()` call so a
// text value reaches the formatting opcode; everything else compiles as-is.
void Compiler::compile_printable_argument(const Expression& argument) {
    const Type& type = expression_type(argument);
    if (type.kind == ValueType::struct_type) {
        const auto to_text = functions_.find(function_key("to_text", {type}));
        if (to_text != functions_.end()) {
            compile_expression(argument);
            emit(OpCode::call, to_text->second);
            return;
        }
    }

    compile_expression(argument);
}

// Lower the low-level stdlib intrinsics (see TypeChecker::is_io_builtin) to
// their dedicated opcodes. Returns false for any non-intrinsic call so the
// normal resolved-call path handles it.
bool Compiler::compile_io_builtin_expression(const Expression& expression) {
    OpCode op = OpCode::halt;
    if (expression.lexeme == "__read_file") {
        op = OpCode::read_file;
    } else if (expression.lexeme == "__write_file") {
        op = OpCode::write_file;
    } else if (expression.lexeme == "__stdout_write") {
        op = OpCode::stdout_write;
    } else if (expression.lexeme == "__stderr_write") {
        op = OpCode::stderr_write;
    } else if (expression.lexeme == "__stdout_flush") {
        op = OpCode::stdout_flush;
    } else if (expression.lexeme == "__stderr_flush") {
        op = OpCode::stderr_flush;
    } else if (expression.lexeme == "__stdin_read_line") {
        op = OpCode::stdin_read_line;
    } else if (expression.lexeme == "__env_get") {
        op = OpCode::env_get;
    } else if (expression.lexeme == "__process_args") {
        op = OpCode::process_args;
    } else if (expression.lexeme == "__process_cwd") {
        op = OpCode::process_cwd;
    } else if (expression.lexeme == "__log_emit") {
        op = OpCode::log_emit;
    } else if (expression.lexeme == "__log_set_level") {
        op = OpCode::log_set_level;
    } else if (expression.lexeme == "__log_level") {
        op = OpCode::log_level;
    } else if (expression.lexeme == "__plot_backend_get") {
        op = OpCode::plot_backend_get;
    } else if (expression.lexeme == "__plot_backend_set") {
        op = OpCode::plot_backend_set;
    } else if (expression.lexeme == "__plot_show_native") {
        op = OpCode::plot_show_native;
    } else if (expression.lexeme == "__canvas_show_native") {
        op = OpCode::canvas_show_native;
    } else {
        return false;
    }

    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        compile_expression(*argument);
    }

    emit(op);
    return true;
}

void Compiler::compile_when_expression(const Expression& expression) {
    const Type subject_type = expression_type(*expression.left);
    compile_expression(*expression.left);
    const std::size_t subject_slot =
        declare_scoped_local("__when_subject_" + std::to_string(temporary_count_++), subject_type);
    emit(OpCode::store_local, subject_slot);

    if (subject_type.kind == ValueType::enum_type) {
        std::vector<std::size_t> end_jumps;
        for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
            const Expression& pattern = *expression.arguments[index];
            const Expression& result = *expression.arguments[index + 1];
            const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

            std::size_t next_when = 0;
            const TypeChecker::VariantResolution* resolution = nullptr;
            if (!wildcard) {
                resolution = &resolved_variants_.at(&pattern);
                emit(OpCode::load_local, subject_slot);
                emit(OpCode::load_variant_tag);
                emit(OpCode::push_constant, add_constant(make_unsigned(resolution->tag)));
                emit(OpCode::equal);
                next_when = emit(OpCode::jump_if_false);
            }

            push_scope();
            if (resolution != nullptr && resolution->binds_payload) {
                emit(OpCode::load_local, subject_slot);
                emit(OpCode::load_variant_payload);
                emit(OpCode::store_local, declare_scoped_local(resolution->binding_name, resolution->payload_type));
            }

            compile_expression(result);
            pop_scope();

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

    if (subject_type.kind == ValueType::struct_type || subject_type.kind == ValueType::tuple_type) {
        if (expression.arguments.size() != 2) {
            throw std::runtime_error("record and tuple destructuring when expressions need exactly one arm");
        }

        const Expression& pattern = *expression.arguments[0];
        const Expression& result = *expression.arguments[1];
        const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

        push_scope();
        if (!wildcard && subject_type.kind == ValueType::struct_type) {
            const auto layout = structs_.find(subject_type.name);
            if (layout == structs_.end()) {
                throw std::runtime_error("unknown record '" + subject_type.name + "'");
            }

            for (std::size_t index = 0; index < pattern.field_names.size(); ++index) {
                const Expression& binding = *pattern.arguments[index];
                if (binding.kind != ExpressionKind::identifier || binding.lexeme == "_") {
                    continue;
                }

                const auto field = layout->second.field_indices.find(pattern.field_names[index]);
                if (field == layout->second.field_indices.end()) {
                    throw std::runtime_error("unknown field '" + pattern.field_names[index] + "'");
                }

                emit(OpCode::load_local, subject_slot);
                emit(OpCode::load_field, field->second);
                emit(OpCode::store_local,
                     declare_scoped_local(binding.lexeme, layout->second.fields[field->second].type.type));
            }
        } else if (!wildcard && subject_type.kind == ValueType::tuple_type) {
            for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
                const Expression& binding = *pattern.arguments[index];
                if (binding.kind != ExpressionKind::identifier || binding.lexeme == "_") {
                    continue;
                }

                emit(OpCode::load_local, subject_slot);
                emit(OpCode::load_tuple_element, index);
                emit(OpCode::store_local, declare_scoped_local(binding.lexeme, subject_type.arguments[index]));
            }
        }

        compile_expression(result);
        pop_scope();
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

        push_scope();
        compile_expression(result);
        pop_scope();
        end_jumps.push_back(emit(OpCode::jump));
        if (!wildcard) {
            patch_operand(next_when, instructions_->size());
        }
    }

    for (const std::size_t jump : end_jumps) {
        patch_operand(jump, instructions_->size());
    }
}

void Compiler::compile_assignment_target(const Expression& target, const Expression& value) {
    switch (target.kind) {
    case ExpressionKind::index:
        compile_expression(*target.left);
        compile_expression(*target.right);
        compile_expression(value);
        emit(OpCode::store_index);
        return;
    case ExpressionKind::member: {
        const Type receiver = expression_type(*target.left);
        if (receiver.kind != ValueType::struct_type) {
            throw std::runtime_error("expected record assignment target");
        }

        const auto layout = structs_.find(receiver.name);
        if (layout == structs_.end()) {
            throw std::runtime_error("unknown record '" + receiver.name + "'");
        }

        const auto field = layout->second.field_indices.find(target.lexeme);
        if (field == layout->second.field_indices.end()) {
            throw std::runtime_error("unknown field '" + target.lexeme + "'");
        }

        compile_expression(*target.left);
        compile_expression(value);
        emit(OpCode::store_field, field->second);
        return;
    }
    case ExpressionKind::identifier:
        compile_expression(value);
        emit(OpCode::store_local, resolve_local(target.lexeme));
        return;
    case ExpressionKind::number:
    case ExpressionKind::floating:
    case ExpressionKind::character:
    case ExpressionKind::string:
    case ExpressionKind::boolean:
    case ExpressionKind::array:
    case ExpressionKind::array_comprehension:
    case ExpressionKind::tuple:
    case ExpressionKind::struct_literal:
    case ExpressionKind::slice:
    case ExpressionKind::unary:
    case ExpressionKind::try_expression:
    case ExpressionKind::cast:
    case ExpressionKind::binary:
    case ExpressionKind::range:
    case ExpressionKind::when_expression:
    case ExpressionKind::call:
    case ExpressionKind::method_call:
        break;
    }

    throw std::runtime_error("invalid assignment target");
}

void Compiler::compile_variant_constructor(const Expression& expression) {
    const TypeChecker::VariantResolution& resolution = resolved_variants_.at(&expression);
    if (resolution.has_payload) {
        if (expression.kind == ExpressionKind::call || expression.kind == ExpressionKind::method_call) {
            compile_expression(*expression.arguments.at(0));
        } else {
            throw std::runtime_error("choice variant payload constructor needs an argument");
        }

        // Push the variant name so the VM can render the choice as text by
        // default; the make_variant opcode pops it (stack: [payload, name]).
        emit(OpCode::push_constant, add_constant(make_text(resolution.variant_name)));
        emit(OpCode::make_variant, resolution.tag);
        return;
    }

    emit(OpCode::push_constant, add_constant(make_text(resolution.variant_name)));
    emit(OpCode::make_unit_variant, resolution.tag);
}

void Compiler::compile_tuple_literal(const Expression& expression) {
    for (const std::unique_ptr<Expression>& element : expression.arguments) {
        compile_expression(*element);
    }

    emit(OpCode::make_tuple, expression.arguments.size());
}

void Compiler::compile_tuple_destructuring_assignment(const Expression& target, const Expression& value) {
    const Type tuple_type = expression_type(value);
    if (tuple_type.kind != ValueType::tuple_type) {
        throw std::runtime_error("expected tuple value");
    }

    compile_expression(value);
    const std::size_t tuple_slot =
        declare_scoped_local("__tuple_destructure_" + std::to_string(temporary_count_++), tuple_type);
    emit(OpCode::store_local, tuple_slot);

    for (std::size_t index = 0; index < target.arguments.size(); ++index) {
        const Expression& binding = *target.arguments[index];
        if (binding.kind != ExpressionKind::identifier || binding.lexeme == "_") {
            continue;
        }

        emit(OpCode::load_local, tuple_slot);
        emit(OpCode::load_tuple_element, index);
        if (const auto local = locals_.find(binding.lexeme); local != locals_.end()) {
            emit(OpCode::store_local, local->second);
        } else {
            emit(OpCode::store_local, declare_scoped_local(binding.lexeme, tuple_type.arguments[index]));
        }
    }
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
            if (field.default_value == nullptr) {
                throw std::runtime_error("missing field '" + field.name + "'");
            }

            compile_expression(*field.default_value);
            continue;
        }

        const std::size_t index = static_cast<std::size_t>(source - expression.field_names.begin());
        compile_expression(*expression.arguments.at(index));
    }

    emit(OpCode::make_record, layout->second.fields.size());
}

void Compiler::compile_method_call_expression(const Expression& expression) {
    if (expression.left != nullptr && expression.left->kind == ExpressionKind::identifier &&
        expression.left->lexeme == "fmt" && expression.lexeme == "format") {
        compile_format_expression(expression);
        return;
    }

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

    if (expression.lexeme == "in") {
        compile_expression(*expression.left);
        compile_expression(*expression.right);

        const Type container = expression_type(*expression.right);
        if (container.kind == ValueType::text_type) {
            emit(OpCode::text_in);
            return;
        }

        emit(OpCode::array_contains);
        return;
    }

    // Overloaded operator: the type checker resolved this `+ - * /` to a record
    // method, so emit a method call (receiver = left, argument = right).
    if (resolved_calls_.contains(&expression)) {
        const std::size_t function_index = resolve_function(resolved_calls_.at(&expression));
        compile_expression(*expression.left);
        compile_expression(*expression.right);
        emit(OpCode::call, function_index);
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

std::size_t Compiler::declare_scoped_local(const std::string& name, const Type& type) {
    if (local_scopes_.empty()) {
        push_scope();
    }

    ScopedLocal local;
    local.name = name;
    const auto previous = locals_.find(name);
    if (previous != locals_.end()) {
        local.had_previous = true;
        local.previous_slot = previous->second;
        local.previous_type = local_types_.at(name);
    }

    const std::size_t slot = local_count_++;
    locals_[name] = slot;
    local_types_[name] = type;
    local_scopes_.back().push_back(std::move(local));
    return slot;
}

void Compiler::reset_scopes() {
    local_scopes_.clear();
    push_scope();
}

void Compiler::push_scope() {
    local_scopes_.emplace_back();
}

void Compiler::pop_scope() {
    if (local_scopes_.empty()) {
        return;
    }

    for (auto local = local_scopes_.back().rbegin(); local != local_scopes_.back().rend(); ++local) {
        if (local->had_previous) {
            locals_[local->name] = local->previous_slot;
            local_types_[local->name] = local->previous_type;
        } else {
            locals_.erase(local->name);
            local_types_.erase(local->name);
        }
    }

    local_scopes_.pop_back();
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

const Type& Compiler::iterable_element_type(const Expression& expression) const {
    const auto existing = iterable_element_types_.find(&expression);
    if (existing == iterable_element_types_.end()) {
        throw std::runtime_error("missing inferred iterable element type");
    }

    return existing->second;
}

Type Compiler::normalize_type(const Type& type) const {
    std::unordered_set<std::string> resolving_aliases;
    return normalize_type(type, resolving_aliases);
}

Type Compiler::normalize_type(const Type& type, std::unordered_set<std::string>& resolving_aliases) const {
    if (type.kind == ValueType::array_type) {
        Type result{ValueType::array_type, nullptr};
        if (type.element != nullptr) {
            result.element = std::make_shared<Type>(normalize_type(*type.element, resolving_aliases));
        }
        return result;
    }

    std::vector<Type> arguments;
    arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        arguments.push_back(normalize_type(argument, resolving_aliases));
    }

    if (type.kind == ValueType::generic_type) {
        const auto alias = type_aliases_.find(type.name);
        if (alias != type_aliases_.end() && arguments.size() == alias->second.generic_parameters.size() &&
            resolving_aliases.insert(type.name).second) {
            std::unordered_map<std::string, Type> substitutions;
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                substitutions.emplace(alias->second.generic_parameters[index].name, arguments[index]);
            }
            Type resolved =
                normalize_type(substitute_type(alias->second.target, substitutions), resolving_aliases);
            resolving_aliases.erase(type.name);
            return resolved;
        }
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
