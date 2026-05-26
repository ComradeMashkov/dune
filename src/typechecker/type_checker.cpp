#include "type_checker.hpp"

#include <stdexcept>

namespace dune {

std::string type_name(ValueType type) {
    switch (type) {
    case ValueType::int_type:
        return "int";
    case ValueType::bool_type:
        return "bool";
    }

    return "unknown";
}

void TypeChecker::check(const Program& program) {
    functions_.clear();

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
        const ValueType actual = check_expression(*statement.expression);
        ValueType expected = statement.type.has_type ? statement.type.type : actual;
        const auto existing = variables_.find(statement.name);
        if (existing != variables_.end()) {
            if (statement.type.has_type) {
                expect_type(existing->second, statement.type.type, statement.location);
            }

            expected = existing->second;
        }

        expect_type(expected, actual, statement.expression->location);
        variables_[statement.name] = expected;
        return;
    }
    case StatementKind::assign: {
        const auto variable = variables_.find(statement.name);
        if (variable == variables_.end()) {
            throw std::runtime_error(diagnostic(statement.location, "undefined variable '" + statement.name + "'"));
        }

        expect_type(variable->second, check_expression(*statement.expression), statement.expression->location);
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

        expect_type(current_function_->return_type, check_expression(*statement.expression),
                    statement.expression->location);
        return;
    }
}

void TypeChecker::check_statements(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        check_statement(statement);
    }
}

ValueType TypeChecker::check_expression(const Expression& expression) {
    switch (expression.kind) {
    case ExpressionKind::identifier: {
        const auto variable = variables_.find(expression.lexeme);
        if (variable == variables_.end()) {
            throw std::runtime_error(diagnostic(expression.location, "undefined variable '" + expression.lexeme + "'"));
        }

        return variable->second;
    }
    case ExpressionKind::number:
        return ValueType::int_type;
    case ExpressionKind::boolean:
        return ValueType::bool_type;
    case ExpressionKind::binary:
        return check_binary_expression(expression);
    case ExpressionKind::call:
        return check_call_expression(expression);
    }

    return ValueType::int_type;
}

ValueType TypeChecker::check_binary_expression(const Expression& expression) {
    const ValueType left = check_expression(*expression.left);
    const ValueType right = check_expression(*expression.right);

    if (expression.lexeme == "+" || expression.lexeme == "-" || expression.lexeme == "*" || expression.lexeme == "/") {
        expect_type(ValueType::int_type, left, expression.left->location);
        expect_type(ValueType::int_type, right, expression.right->location);
        return ValueType::int_type;
    }

    if (expression.lexeme == ">" || expression.lexeme == ">=" || expression.lexeme == "<" ||
        expression.lexeme == "<=") {
        expect_type(ValueType::int_type, left, expression.left->location);
        expect_type(ValueType::int_type, right, expression.right->location);
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
        const ValueType actual = check_expression(*expression.arguments[index]);
        const ValueType expected = function->second.parameters[index];
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
