#include "type_checker.hpp"

#include <limits>
#include <stdexcept>

namespace dune {

namespace {

TypeAnnotation expected_type(ValueType type) {
    return TypeAnnotation{true, type};
}

} // namespace

std::string type_name(ValueType type) {
    switch (type) {
    case ValueType::int_type:
        return "int";
    case ValueType::bool_type:
        return "bool";
    case ValueType::u8_type:
        return "u8";
    case ValueType::u16_type:
        return "u16";
    case ValueType::u32_type:
        return "u32";
    case ValueType::u64_type:
        return "u64";
    case ValueType::real_type:
        return "real";
    case ValueType::glyph_type:
        return "glyph";
    }

    return "unknown";
}

void TypeChecker::check(const Program& program) {
    functions_.clear();
    expression_types_.clear();

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            collect_function(statement);
        }
    }

    variables_.clear();
    current_function_ = nullptr;
    for (const Statement& statement : program.statements) {
        if (statement.kind != StatementKind::function) {
            check_statement(statement);
        }
    }

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            check_function(statement);
        }
    }
}

const std::unordered_map<const Expression*, ValueType>& TypeChecker::expression_types() const {
    return expression_types_;
}

void TypeChecker::collect_function(const Statement& statement) {
    if (functions_.contains(statement.name)) {
        throw std::runtime_error(diagnostic(statement.location, "duplicate function '" + statement.name + "'"));
    }

    FunctionSignature signature;
    signature.name = statement.name;
    signature.return_type = annotation_or_default(statement.type);
    signature.location = statement.location;

    for (const Parameter& parameter : statement.parameters) {
        signature.parameters.push_back(annotation_or_default(parameter.type));
    }

    functions_.emplace(statement.name, std::move(signature));
}

void TypeChecker::check_function(const Statement& statement) {
    auto function = functions_.find(statement.name);
    if (function == functions_.end()) {
        throw std::runtime_error(diagnostic(statement.location, "undefined function '" + statement.name + "'"));
    }

    variables_.clear();
    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        const Parameter& parameter = statement.parameters[index];
        if (variables_.contains(parameter.name)) {
            throw std::runtime_error(diagnostic(parameter.location, "duplicate parameter '" + parameter.name + "'"));
        }

        variables_.emplace(parameter.name, function->second.parameters[index]);
    }

    current_function_ = &function->second;
    check_statements(statement.body);
    if (!statements_return(statement.body)) {
        throw std::runtime_error(diagnostic(statement.location, "function '" + statement.name + "' must return type '" +
                                                                    type_name(function->second.return_type) + "'"));
    }

    current_function_ = nullptr;
}

void TypeChecker::check_statement(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::let: {
        TypeAnnotation expected = statement.type;
        ValueType actual = check_expression(*statement.expression, expected);
        if (!expected.has_type) {
            expected = expected_type(actual);
        }

        const auto existing = variables_.find(statement.name);
        if (existing != variables_.end()) {
            expect_type(existing->second, expected.type, statement.location);
            expected = expected_type(existing->second);
            actual = check_expression(*statement.expression, expected);
        }

        expect_type(expected.type, actual, statement.expression->location);
        variables_[statement.name] = expected.type;
        return;
    }
    case StatementKind::assign: {
        const auto variable = variables_.find(statement.name);
        if (variable == variables_.end()) {
            throw std::runtime_error(diagnostic(statement.location, "undefined variable '" + statement.name + "'"));
        }

        expect_type(variable->second, check_expression(*statement.expression, expected_type(variable->second)),
                    statement.expression->location);
        return;
    }
    case StatementKind::print:
        check_expression(*statement.expression);
        return;
    case StatementKind::block:
        check_statements(statement.body);
        return;
    case StatementKind::if_statement:
        expect_type(ValueType::bool_type, check_expression(*statement.expression), statement.expression->location);
        check_statements(statement.body);
        check_statements(statement.else_body);
        return;
    case StatementKind::while_statement:
        expect_type(ValueType::bool_type, check_expression(*statement.expression), statement.expression->location);
        check_statements(statement.body);
        return;
    case StatementKind::function:
        throw std::runtime_error(diagnostic(statement.location, "function declarations are only allowed at top level"));
    case StatementKind::return_statement:
        if (current_function_ == nullptr) {
            throw std::runtime_error(diagnostic(statement.location, "return statement outside function"));
        }

        expect_type(current_function_->return_type,
                    check_expression(*statement.expression, expected_type(current_function_->return_type)),
                    statement.expression->location);
        return;
    }
}

void TypeChecker::check_statements(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        check_statement(statement);
    }
}

ValueType TypeChecker::check_expression(const Expression& expression, const TypeAnnotation& expected) {
    ValueType actual = ValueType::int_type;

    switch (expression.kind) {
    case ExpressionKind::identifier: {
        const auto variable = variables_.find(expression.lexeme);
        if (variable == variables_.end()) {
            throw std::runtime_error(diagnostic(expression.location, "undefined variable '" + expression.lexeme + "'"));
        }

        actual = variable->second;
        break;
    }
    case ExpressionKind::number:
        actual = expected.has_type && is_numeric_type(expected.type) ? expected.type : ValueType::int_type;
        check_integer_literal_range(expression, actual);
        break;
    case ExpressionKind::floating:
        actual = ValueType::real_type;
        break;
    case ExpressionKind::character:
        actual = ValueType::glyph_type;
        break;
    case ExpressionKind::boolean:
        actual = ValueType::bool_type;
        break;
    case ExpressionKind::binary:
        actual = check_binary_expression(expression, expected);
        break;
    case ExpressionKind::call:
        actual = check_call_expression(expression);
        break;
    }

    if (expected.has_type && actual != expected.type) {
        actual = coerce_numeric_literal(expression, actual, expected.type);
    }

    if (expected.has_type) {
        expect_type(expected.type, actual, expression.location);
    }

    expression_types_[&expression] = actual;
    return actual;
}

ValueType TypeChecker::check_binary_expression(const Expression& expression, const TypeAnnotation& expected) {
    const bool is_arithmetic =
        expression.lexeme == "+" || expression.lexeme == "-" || expression.lexeme == "*" || expression.lexeme == "/";
    const TypeAnnotation operand_expected =
        is_arithmetic && expected.has_type && is_numeric_type(expected.type) ? expected : TypeAnnotation{};

    ValueType left = check_expression(*expression.left, operand_expected);
    ValueType right = check_expression(*expression.right, operand_expected);

    if (left != right) {
        left = coerce_numeric_literal(*expression.left, left, right);
        right = coerce_numeric_literal(*expression.right, right, left);
    }

    if (is_arithmetic) {
        if (!is_numeric_type(left)) {
            throw std::runtime_error(
                diagnostic(expression.left->location, "expected numeric type but got '" + type_name(left) + "'"));
        }

        expect_type(left, right, expression.right->location);
        return left;
    }

    if (expression.lexeme == ">" || expression.lexeme == ">=" || expression.lexeme == "<" ||
        expression.lexeme == "<=") {
        if (!is_numeric_type(left)) {
            throw std::runtime_error(
                diagnostic(expression.left->location, "expected numeric type but got '" + type_name(left) + "'"));
        }

        expect_type(left, right, expression.right->location);
        return ValueType::bool_type;
    }

    if (expression.lexeme == "==" || expression.lexeme == "!=") {
        expect_type(left, right, expression.right->location);
        return ValueType::bool_type;
    }

    throw std::runtime_error(diagnostic(expression.location, "unknown binary operator '" + expression.lexeme + "'"));
}

ValueType TypeChecker::check_call_expression(const Expression& expression) {
    const auto function = functions_.find(expression.lexeme);
    if (function == functions_.end()) {
        throw std::runtime_error(diagnostic(expression.location, "undefined function '" + expression.lexeme + "'"));
    }

    if (expression.arguments.size() != function->second.parameters.size()) {
        throw std::runtime_error(
            diagnostic(expression.location, "function '" + expression.lexeme + "' expects " +
                                                std::to_string(function->second.parameters.size()) +
                                                " arguments but got " + std::to_string(expression.arguments.size())));
    }

    for (std::size_t index = 0; index < expression.arguments.size(); ++index) {
        const ValueType expected = function->second.parameters[index];
        const ValueType actual = check_expression(*expression.arguments[index], expected_type(expected));
        if (actual != expected) {
            throw std::runtime_error(diagnostic(expression.arguments[index]->location,
                                                "argument " + std::to_string(index + 1) + " for function '" +
                                                    expression.lexeme + "': expected type '" + type_name(expected) +
                                                    "' but got '" + type_name(actual) + "'"));
        }
    }

    return function->second.return_type;
}

bool TypeChecker::statement_returns(const Statement& statement) const {
    switch (statement.kind) {
    case StatementKind::return_statement:
        return true;
    case StatementKind::block:
        return statements_return(statement.body);
    case StatementKind::if_statement:
        return !statement.else_body.empty() && statements_return(statement.body) &&
               statements_return(statement.else_body);
    case StatementKind::let:
    case StatementKind::assign:
    case StatementKind::print:
    case StatementKind::while_statement:
    case StatementKind::function:
        return false;
    }

    return false;
}

bool TypeChecker::statements_return(const std::vector<Statement>& statements) const {
    for (const Statement& statement : statements) {
        if (statement_returns(statement)) {
            return true;
        }
    }

    return false;
}

ValueType TypeChecker::annotation_or_default(const TypeAnnotation& annotation) const {
    if (annotation.has_type) {
        return annotation.type;
    }

    return ValueType::int_type;
}

bool TypeChecker::is_integer_type(ValueType type) const {
    return type == ValueType::int_type || is_unsigned_type(type);
}

bool TypeChecker::is_unsigned_type(ValueType type) const {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type;
}

bool TypeChecker::is_numeric_type(ValueType type) const {
    return is_integer_type(type) || type == ValueType::real_type;
}

bool TypeChecker::can_coerce_integer_literal(const Expression& expression, ValueType target) const {
    return expression.kind == ExpressionKind::number && is_numeric_type(target);
}

void TypeChecker::check_integer_literal_range(const Expression& expression, ValueType target) const {
    if (!is_unsigned_type(target)) {
        return;
    }

    const unsigned long long value = std::stoull(expression.lexeme);
    unsigned long long max = std::numeric_limits<unsigned long long>::max();
    if (target == ValueType::u8_type) {
        max = std::numeric_limits<unsigned char>::max();
    } else if (target == ValueType::u16_type) {
        max = std::numeric_limits<unsigned short>::max();
    } else if (target == ValueType::u32_type) {
        max = std::numeric_limits<unsigned int>::max();
    }

    if (value > max) {
        throw std::runtime_error(
            diagnostic(expression.location,
                       "integer literal '" + expression.lexeme + "' does not fit in type '" + type_name(target) + "'"));
    }
}

ValueType TypeChecker::coerce_numeric_literal(const Expression& expression, ValueType actual, ValueType target) {
    if (actual != target && can_coerce_integer_literal(expression, target)) {
        check_integer_literal_range(expression, target);
        expression_types_[&expression] = target;
        return target;
    }

    return actual;
}

void TypeChecker::expect_type(ValueType expected, ValueType actual, SourceLocation location) const {
    if (expected != actual) {
        throw std::runtime_error(
            diagnostic(location, "expected type '" + type_name(expected) + "' but got '" + type_name(actual) + "'"));
    }
}

std::string TypeChecker::diagnostic(SourceLocation location, const std::string& message) const {
    return "line " + std::to_string(location.line) + ", column " + std::to_string(location.column) + ": " + message;
}

} // namespace dune
