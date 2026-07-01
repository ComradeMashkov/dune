#include "parser.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace dune {

namespace {

bool is_identifier_like_token(TokenType type) {
    return type == TokenType::identifier || type == TokenType::text_keyword;
}

SourceLocation location_from_token(const Token& token) {
    return SourceLocation{token.line, token.column, token.lexeme.empty() ? 1 : token.lexeme.size()};
}

std::unique_ptr<Expression> make_leaf(ExpressionKind kind, std::string lexeme, SourceLocation location) {
    Expression expression{kind, std::move(lexeme), nullptr, nullptr};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_binary(std::unique_ptr<Expression> left, std::string lexeme,
                                        std::unique_ptr<Expression> right, SourceLocation location) {
    Expression expression{ExpressionKind::binary, std::move(lexeme), std::move(left), std::move(right)};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_range(std::unique_ptr<Expression> start, std::unique_ptr<Expression> end,
                                       SourceLocation location) {
    Expression expression{ExpressionKind::range, "..", std::move(start), std::move(end)};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_unary(std::string lexeme, std::unique_ptr<Expression> right, SourceLocation location) {
    Expression expression{ExpressionKind::unary, std::move(lexeme), nullptr, std::move(right)};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_try(std::unique_ptr<Expression> operand, SourceLocation location) {
    auto expression =
        std::make_unique<Expression>(Expression{ExpressionKind::try_expression, "?", std::move(operand), nullptr});
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_cast(std::unique_ptr<Expression> value, TypeAnnotation target_type,
                                      SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::cast, "", std::move(value), nullptr});
    expression->type = std::move(target_type);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_call(std::string name, std::vector<std::unique_ptr<Expression>> arguments,
                                      SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::call, std::move(name), nullptr, nullptr});
    expression->arguments = std::move(arguments);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_method_call(std::unique_ptr<Expression> receiver, std::string name,
                                             std::vector<std::unique_ptr<Expression>> arguments,
                                             SourceLocation location) {
    auto expression = std::make_unique<Expression>(
        Expression{ExpressionKind::method_call, std::move(name), std::move(receiver), nullptr});
    expression->arguments = std::move(arguments);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_member(std::unique_ptr<Expression> receiver, std::string name,
                                        SourceLocation location) {
    auto expression =
        std::make_unique<Expression>(Expression{ExpressionKind::member, std::move(name), std::move(receiver), nullptr});
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_array(std::vector<std::unique_ptr<Expression>> elements, SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::array, "", nullptr, nullptr});
    expression->arguments = std::move(elements);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_array_comprehension(std::unique_ptr<Expression> body, std::string variable,
                                                     std::unique_ptr<Expression> iterable,
                                                     std::unique_ptr<Expression> condition, SourceLocation location) {
    auto expression = std::make_unique<Expression>(
        Expression{ExpressionKind::array_comprehension, std::move(variable), std::move(body), std::move(iterable)});
    if (condition != nullptr) {
        expression->arguments.push_back(std::move(condition));
    }

    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_tuple(std::vector<std::unique_ptr<Expression>> elements, SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::tuple, "", nullptr, nullptr});
    expression->arguments = std::move(elements);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_struct_literal(std::string name, std::vector<std::string> field_names,
                                                std::vector<std::unique_ptr<Expression>> values,
                                                SourceLocation location) {
    auto expression =
        std::make_unique<Expression>(Expression{ExpressionKind::struct_literal, std::move(name), nullptr, nullptr});
    expression->field_names = std::move(field_names);
    expression->arguments = std::move(values);
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_index(std::unique_ptr<Expression> array, std::unique_ptr<Expression> index,
                                       SourceLocation location) {
    Expression expression{ExpressionKind::index, "", std::move(array), std::move(index)};
    expression.location = location;
    return std::make_unique<Expression>(std::move(expression));
}

std::unique_ptr<Expression> make_slice(std::unique_ptr<Expression> value, std::unique_ptr<Expression> start,
                                       std::unique_ptr<Expression> end, SourceLocation location) {
    auto expression = std::make_unique<Expression>(Expression{ExpressionKind::slice, "", std::move(value), nullptr});
    expression->arguments.push_back(std::move(start));
    expression->arguments.push_back(std::move(end));
    expression->location = location;
    return expression;
}

std::unique_ptr<Expression> make_match(std::unique_ptr<Expression> subject,
                                       std::vector<std::unique_ptr<Expression>> cases, SourceLocation location) {
    auto expression =
        std::make_unique<Expression>(Expression{ExpressionKind::when_expression, "", std::move(subject), nullptr});
    expression->arguments = std::move(cases);
    expression->location = location;
    return expression;
}

Type make_type(ValueType type) {
    return Type{type, nullptr};
}

Type make_array_type(Type element) {
    return Type{ValueType::array_type, std::make_shared<Type>(std::move(element))};
}

Type make_tuple_type(std::vector<Type> elements) {
    Type type{ValueType::tuple_type, nullptr};
    type.arguments = std::move(elements);
    return type;
}

Type make_generic_type(std::string name) {
    Type type{ValueType::generic_type, nullptr};
    type.name = std::move(name);
    return type;
}

Type with_type_arguments(Type type, std::vector<Type> arguments) {
    type.arguments = std::move(arguments);
    return type;
}

Type make_named_type(std::string name, const std::vector<GenericParameter>& generic_parameters) {
    std::vector<Type> arguments;
    arguments.reserve(generic_parameters.size());
    for (const GenericParameter& parameter : generic_parameters) {
        arguments.push_back(make_generic_type(parameter.name));
    }

    return with_type_arguments(make_generic_type(std::move(name)), std::move(arguments));
}

// --- `derive` support -------------------------------------------------------
// Derived methods are synthesised as ordinary record methods, so the type
// checker and compiler need no special cases: `derive` is pure sugar.

std::unique_ptr<Expression> make_field_access(std::string receiver, std::string field, SourceLocation location) {
    return make_member(make_leaf(ExpressionKind::identifier, std::move(receiver), location), std::move(field),
                       location);
}

Statement make_return_method(std::string name, std::vector<Parameter> parameters, TypeAnnotation return_type,
                             std::unique_ptr<Expression> return_value, SourceLocation location) {
    Statement return_statement{StatementKind::return_statement, "", std::move(return_value), {}, {}};
    return_statement.location = location;

    Statement method{StatementKind::function, std::move(name), nullptr, {}, {}};
    method.parameters = std::move(parameters);
    method.type = std::move(return_type);
    method.body.push_back(std::move(return_statement));
    method.exported = true;
    method.location = location;
    return method;
}

bool record_defines_method(const std::vector<Statement>& methods, const std::string& name) {
    for (const Statement& method : methods) {
        if (method.kind == StatementKind::function && method.name == name) {
            return true;
        }
    }

    return false;
}

// `derive debug` -> `to_text(): text { return format("Name(f: {}, ...)", this.f, ...); }`
// Braces are avoided because the format string only understands `{}` placeholders.
Statement make_derived_to_text(const std::string& record_name, const std::vector<Parameter>& fields,
                               SourceLocation location) {
    std::string format = "\"" + record_name + "(";
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            format += ", ";
        }

        format += fields[index].name + ": {}";
    }
    format += ")\"";

    std::vector<std::unique_ptr<Expression>> arguments;
    arguments.push_back(make_leaf(ExpressionKind::string, std::move(format), location));
    for (const Parameter& field : fields) {
        arguments.push_back(make_field_access("this", field.name, location));
    }

    return make_return_method("to_text", {}, TypeAnnotation{true, make_type(ValueType::text_type)},
                              make_call("format", std::move(arguments), location), location);
}

// `derive eq` -> `equals(other: Name): bool { return this.f == other.f && ...; }`
Statement make_derived_equals(const std::string& record_name, const std::vector<GenericParameter>& generics,
                              const std::vector<Parameter>& fields, SourceLocation location) {
    std::unique_ptr<Expression> condition;
    for (const Parameter& field : fields) {
        std::unique_ptr<Expression> comparison =
            make_binary(make_field_access("this", field.name, location),
                        "==", make_field_access("other", field.name, location), location);
        condition = condition == nullptr ? std::move(comparison)
                                         : make_binary(std::move(condition), "&&", std::move(comparison), location);
    }

    if (condition == nullptr) {
        condition = make_leaf(ExpressionKind::boolean, "true", location);
    }

    std::vector<Parameter> parameters;
    parameters.push_back(Parameter{"other", TypeAnnotation{true, make_named_type(record_name, generics)}, location});
    return make_return_method("equals", std::move(parameters), TypeAnnotation{true, make_type(ValueType::bool_type)},
                              std::move(condition), location);
}

// `derive copy` -> `copy(): Name { return Name { f: this.f, ... }; }` (shallow copy)
Statement make_derived_copy(const std::string& record_name, const std::vector<GenericParameter>& generics,
                            const std::vector<Parameter>& fields, SourceLocation location) {
    std::vector<std::string> field_names;
    std::vector<std::unique_ptr<Expression>> values;
    for (const Parameter& field : fields) {
        field_names.push_back(field.name);
        values.push_back(make_field_access("this", field.name, location));
    }

    return make_return_method("copy", {}, TypeAnnotation{true, make_named_type(record_name, generics)},
                              make_struct_literal(record_name, std::move(field_names), std::move(values), location),
                              location);
}

std::string expression_to_type_name(const Expression& expression) {
    if (expression.kind == ExpressionKind::identifier) {
        return expression.lexeme;
    }

    if (expression.kind == ExpressionKind::member && expression.left != nullptr) {
        return expression_to_type_name(*expression.left) + "." + expression.lexeme;
    }

    throw std::runtime_error("expected type name before record literal");
}

std::string decode_string_literal(const std::string& lexeme) {
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
            throw std::runtime_error("invalid string literal");
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
            throw std::runtime_error("unknown string escape");
        }
    }

    return result;
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parse() {
    Program program;

    while (!is_at_end()) {
        program.statements.push_back(statement());
    }

    return program;
}

bool Parser::is_at_end() const {
    return peek().type == TokenType::eof;
}

bool Parser::check(TokenType type) const {
    if (is_at_end()) {
        return type == TokenType::eof;
    }

    return peek().type == type;
}

bool Parser::check_next(TokenType type) const {
    if (current_ + 1 >= tokens_.size()) {
        return false;
    }

    return tokens_[current_ + 1].type == type;
}

bool Parser::check_identifier_like() const {
    return is_identifier_like_token(peek().type);
}

bool Parser::match(TokenType type) {
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool Parser::looks_like_struct_literal() const {
    if (!check(TokenType::left_brace) || current_ + 1 >= tokens_.size()) {
        return false;
    }

    const TokenType first = tokens_[current_ + 1].type;
    if (first == TokenType::right_brace) {
        return true;
    }

    return first == TokenType::identifier && current_ + 2 < tokens_.size() &&
           tokens_[current_ + 2].type == TokenType::colon;
}

bool Parser::looks_like_assignment_statement() const {
    if (!check_identifier_like()) {
        return false;
    }

    int bracket_depth = 0;
    for (std::size_t index = current_ + 1; index < tokens_.size(); ++index) {
        const TokenType type = tokens_[index].type;
        if (type == TokenType::left_bracket) {
            ++bracket_depth;
        } else if (type == TokenType::right_bracket && bracket_depth > 0) {
            --bracket_depth;
        } else if (type == TokenType::equal && bracket_depth == 0) {
            return true;
        } else if ((type == TokenType::semicolon || type == TokenType::eof || type == TokenType::left_brace ||
                    type == TokenType::right_brace || type == TokenType::left_paren) &&
                   bracket_depth == 0) {
            return false;
        }
    }

    return false;
}

bool Parser::looks_like_tuple_assignment_statement() const {
    if (!check(TokenType::left_paren)) {
        return false;
    }

    std::size_t index = current_ + 1;
    std::size_t names = 0;
    bool expect_name = true;
    while (index < tokens_.size()) {
        if (expect_name) {
            if (tokens_[index].type != TokenType::identifier) {
                return false;
            }
            ++names;
            ++index;
            expect_name = false;
            continue;
        }

        if (tokens_[index].type == TokenType::comma) {
            ++index;
            if (index < tokens_.size() && tokens_[index].type == TokenType::right_paren) {
                ++index;
                return names >= 2 && index < tokens_.size() && tokens_[index].type == TokenType::equal;
            }
            expect_name = true;
            continue;
        }

        if (tokens_[index].type == TokenType::right_paren) {
            ++index;
            return names >= 2 && index < tokens_.size() && tokens_[index].type == TokenType::equal;
        }

        return false;
    }

    return false;
}

bool can_start_struct_literal(const Expression& expression) {
    if (expression.kind == ExpressionKind::identifier) {
        return true;
    }

    return expression.kind == ExpressionKind::member && expression.left != nullptr &&
           can_start_struct_literal(*expression.left);
}

bool Parser::looks_like_binding_declaration() const {
    if (!check_identifier_like() || current_ + 1 >= tokens_.size()) {
        return false;
    }

    std::size_t index = current_ + 1;
    if (tokens_[index].type != TokenType::colon) {
        return false;
    }

    int angle_depth = 0;
    int bracket_depth = 0;
    ++index;
    while (index < tokens_.size()) {
        const TokenType type = tokens_[index].type;
        if (type == TokenType::less) {
            ++angle_depth;
        } else if (type == TokenType::greater && angle_depth > 0) {
            --angle_depth;
        } else if (type == TokenType::left_bracket) {
            ++bracket_depth;
        } else if (type == TokenType::right_bracket && bracket_depth > 0) {
            --bracket_depth;
        } else if (type == TokenType::equal && angle_depth == 0 && bracket_depth == 0) {
            return true;
        } else if ((type == TokenType::semicolon || type == TokenType::eof || type == TokenType::left_brace ||
                    type == TokenType::right_brace) &&
                   angle_depth == 0 && bracket_depth == 0) {
            return false;
        }

        ++index;
    }

    return false;
}

const Token& Parser::advance() {
    if (!is_at_end()) {
        ++current_;
    }

    return previous();
}

const Token& Parser::peek() const {
    return tokens_[current_];
}

const Token& Parser::previous() const {
    return tokens_[current_ - 1];
}

const Token& Parser::consume(TokenType type, std::string_view message) {
    if (check(type)) {
        return advance();
    }

    throw std::runtime_error(std::string(message));
}

Token Parser::consume_identifier_like(std::string_view message) {
    if (check_identifier_like()) {
        return advance();
    }

    throw std::runtime_error(std::string(message));
}

bool Parser::match_identifier_like() {
    if (check_identifier_like()) {
        advance();
        return true;
    }

    return false;
}

Statement Parser::statement() {
    if (match(TokenType::export_keyword)) {
        return export_statement();
    }

    if (match(TokenType::foreign_keyword)) {
        return extern_statement();
    }

    if (match(TokenType::fn_keyword)) {
        return function_statement();
    }

    if (match(TokenType::method_keyword)) {
        return method_statement();
    }

    if (match(TokenType::record_keyword)) {
        return struct_statement();
    }

    if (match(TokenType::contract_keyword)) {
        return contract_statement();
    }

    if (match(TokenType::choice_keyword)) {
        return enum_statement();
    }

    if (match(TokenType::import_keyword)) {
        return import_statement();
    }

    if (match(TokenType::type_keyword)) {
        return type_alias_statement();
    }

    if (looks_like_binding_declaration()) {
        return binding_statement();
    }

    if (match(TokenType::const_keyword)) {
        return const_statement();
    }

    if (match(TokenType::return_keyword)) {
        return return_statement();
    }

    if (match(TokenType::print)) {
        return print_statement();
    }

    if (match(TokenType::if_keyword)) {
        return if_statement();
    }

    if (match(TokenType::while_keyword)) {
        return while_statement();
    }

    if (match(TokenType::for_keyword)) {
        return for_statement();
    }

    if (match(TokenType::break_keyword)) {
        return break_statement();
    }

    if (match(TokenType::continue_keyword)) {
        return continue_statement();
    }

    if (match(TokenType::left_brace)) {
        return block_statement();
    }

    if (looks_like_assignment_statement()) {
        return assignment_statement();
    }

    if (looks_like_tuple_assignment_statement()) {
        return tuple_assignment_statement();
    }

    std::unique_ptr<Expression> value = expression();
    if (!match(TokenType::semicolon) && !check(TokenType::right_brace) && !check(TokenType::eof)) {
        throw std::runtime_error("expected ';' after expression statement");
    }

    Statement statement{StatementKind::expression_statement, "", std::move(value), {}, {}};
    statement.location = statement.expression->location;
    return statement;
}

Statement Parser::assignment_statement(bool require_semicolon) {
    std::unique_ptr<Expression> target = assignment_target();
    consume(TokenType::equal, "expected '=' after assignment target");
    std::unique_ptr<Expression> value = expression();
    if (require_semicolon) {
        consume(TokenType::semicolon, "expected ';' after assignment");
    }

    std::string name;
    if (target->kind == ExpressionKind::identifier) {
        name = target->lexeme;
    }

    Statement statement{StatementKind::assign, std::move(name), std::move(value), {}, {}};
    statement.location = target->location;
    statement.target = std::move(target);
    return statement;
}

Statement Parser::tuple_assignment_statement() {
    const Token& start = consume(TokenType::left_paren, "expected '(' before tuple destructuring target");
    std::vector<std::unique_ptr<Expression>> targets;
    auto parse_target = [&]() {
        const Token& name = consume(TokenType::identifier, "expected binding name in tuple destructuring target");
        targets.push_back(make_leaf(ExpressionKind::identifier, name.lexeme, location_from_token(name)));
    };

    parse_target();
    consume(TokenType::comma, "expected ',' in tuple destructuring target");
    if (check(TokenType::right_paren)) {
        throw std::runtime_error("tuple destructuring target needs at least two bindings");
    }
    do {
        parse_target();
    } while (match(TokenType::comma) && !check(TokenType::right_paren));

    consume(TokenType::right_paren, "expected ')' after tuple destructuring target");
    consume(TokenType::equal, "expected '=' after tuple destructuring target");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after tuple destructuring assignment");

    Statement statement{StatementKind::assign, "", std::move(value), {}, {}};
    statement.location = location_from_token(start);
    statement.target = make_tuple(std::move(targets), location_from_token(start));
    return statement;
}

Statement Parser::block_statement() {
    const Token& brace = previous();
    Statement statement{StatementKind::block, "", nullptr, block(), {}};
    statement.location = location_from_token(brace);
    return statement;
}

Statement Parser::break_statement() {
    const Token& keyword = previous();
    consume(TokenType::semicolon, "expected ';' after break");

    Statement statement{StatementKind::break_statement, "", nullptr, {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::continue_statement() {
    const Token& keyword = previous();
    consume(TokenType::semicolon, "expected ';' after continue");

    Statement statement{StatementKind::continue_statement, "", nullptr, {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::export_statement() {
    if (match(TokenType::foreign_keyword)) {
        Statement statement = extern_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::fn_keyword)) {
        Statement statement = function_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::method_keyword)) {
        Statement statement = method_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::record_keyword)) {
        Statement statement = struct_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::contract_keyword)) {
        Statement statement = contract_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::choice_keyword)) {
        Statement statement = enum_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::const_keyword)) {
        Statement statement = const_statement();
        statement.exported = true;
        return statement;
    }

    if (match(TokenType::type_keyword)) {
        Statement statement = type_alias_statement();
        statement.exported = true;
        return statement;
    }

    throw std::runtime_error(
        "expected fn, foreign function, record, contract, choice, method, constant, or type alias after export");
}

Statement Parser::extern_statement() {
    consume(TokenType::fn_keyword, "expected 'fn' after foreign");
    return function_statement(true);
}

Statement Parser::for_statement() {
    const Token& keyword = previous();
    if (check_identifier_like() && check_next(TokenType::in_keyword)) {
        return for_in_statement(keyword);
    }

    auto statement = Statement{StatementKind::for_statement, "", nullptr, {}, {}};
    statement.location = location_from_token(keyword);

    if (match(TokenType::semicolon)) {
        statement.initializer = nullptr;
    } else if (looks_like_binding_declaration()) {
        statement.initializer = std::make_unique<Statement>(binding_statement());
    } else if (looks_like_assignment_statement()) {
        statement.initializer = std::make_unique<Statement>(assignment_statement());
    } else {
        throw std::runtime_error("expected for initializer");
    }

    if (check(TokenType::semicolon)) {
        statement.expression = make_leaf(ExpressionKind::boolean, "true", location_from_token(peek()));
    } else {
        statement.expression = expression();
    }
    consume(TokenType::semicolon, "expected ';' after for condition");

    if (!check(TokenType::left_brace)) {
        if (looks_like_assignment_statement()) {
            statement.increment = std::make_unique<Statement>(assignment_statement(false));
        } else {
            std::unique_ptr<Expression> value = expression();
            Statement increment{StatementKind::expression_statement, "", std::move(value), {}, {}};
            increment.location = increment.expression->location;
            statement.increment = std::make_unique<Statement>(std::move(increment));
        }
    }

    consume(TokenType::left_brace, "expected '{' before for body");
    statement.body = block();
    return statement;
}

Statement Parser::for_in_statement(const Token& keyword) {
    const Token name = consume_identifier_like("expected loop variable after for");
    consume(TokenType::in_keyword, "expected 'in' after loop variable");
    std::unique_ptr<Expression> iterable = expression();
    consume(TokenType::left_brace, "expected '{' before for body");

    Statement statement{StatementKind::for_in_statement, name.lexeme, std::move(iterable), block(), {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::function_statement(bool is_extern) {
    const Token name = consume_identifier_like("expected function name after fn");
    return finish_function_statement(name, {}, is_extern);
}

Statement Parser::finish_function_statement(const Token& name, std::vector<GenericParameter> leading_generics,
                                            bool is_extern) {
    std::vector<GenericParameter> parsed_generics;
    parsed_generics = std::move(leading_generics);
    if (match(TokenType::less)) {
        std::vector<GenericParameter> function_generics = generic_parameters();
        parsed_generics.insert(parsed_generics.end(), function_generics.begin(), function_generics.end());
        consume(TokenType::greater, "expected '>' after generic parameters");
    }

    consume(TokenType::left_paren, "expected '(' after function name");
    std::vector<Parameter> parsed_parameters = parameters();
    consume(TokenType::right_paren, "expected ')' after function parameters");

    TypeAnnotation return_type;
    if (match(TokenType::colon)) {
        return_type = type_annotation();
    }

    if (is_extern) {
        std::string extern_symbol;
        if (match(TokenType::equal)) {
            const Token& symbol = consume(TokenType::string_literal, "expected foreign symbol string after '='");
            extern_symbol = decode_string_literal(symbol.lexeme);
        }

        consume(TokenType::semicolon, "expected ';' after foreign function declaration");

        Statement statement{StatementKind::function, name.lexeme, nullptr, {}, {}};
        statement.type = return_type;
        statement.parameters = std::move(parsed_parameters);
        statement.generic_parameters = std::move(parsed_generics);
        statement.location = location_from_token(name);
        statement.is_extern = true;
        statement.extern_symbol = std::move(extern_symbol);
        return statement;
    }

    consume(TokenType::left_brace, "expected '{' before function body");

    Statement statement{StatementKind::function, name.lexeme, nullptr, block(), {}};

    statement.type = return_type;
    statement.parameters = std::move(parsed_parameters);
    statement.generic_parameters = std::move(parsed_generics);
    statement.location = location_from_token(name);
    return statement;
}

Statement Parser::method_statement() {
    const Token& keyword = previous();
    std::vector<GenericParameter> parsed_generics;
    if (match(TokenType::less)) {
        parsed_generics = generic_parameters();
        consume(TokenType::greater, "expected '>' after method generic parameters");
    }

    TypeAnnotation receiver_type = type_annotation();
    consume(TokenType::dot, "expected '.' between method receiver and method name");
    const Token method_name = consume_identifier_like("expected method name");
    Statement method = finish_function_statement(method_name, parsed_generics);

    Statement statement{StatementKind::method_block, "", nullptr, {}, {}};
    statement.type = std::move(receiver_type);
    statement.body.push_back(std::move(method));
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::const_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected constant name after const");
    TypeAnnotation declared_type = optional_type_annotation();
    consume(TokenType::equal, "expected '=' after constant name");
    std::unique_ptr<Expression> value = expression();
    consume(TokenType::semicolon, "expected ';' after const statement");

    Statement statement{StatementKind::const_statement, name.lexeme, std::move(value), {}, {}};
    statement.type = declared_type;
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::contract_method_signature() {
    const Token name = consume_identifier_like("expected contract method name");
    consume(TokenType::left_paren, "expected '(' after contract method name");
    std::vector<Parameter> parsed_parameters = parameters();
    consume(TokenType::right_paren, "expected ')' after contract method parameters");

    TypeAnnotation return_type;
    if (match(TokenType::colon)) {
        return_type = type_annotation();
    }

    consume(TokenType::semicolon, "expected ';' after contract method signature");

    Statement method{StatementKind::function, name.lexeme, nullptr, {}, {}};
    method.type = std::move(return_type);
    method.parameters = std::move(parsed_parameters);
    method.location = location_from_token(name);
    return method;
}

Statement Parser::contract_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected contract name after contract");
    consume(TokenType::left_brace, "expected '{' before contract methods");

    std::vector<Statement> methods;
    while (!check(TokenType::right_brace) && !is_at_end()) {
        methods.push_back(contract_method_signature());
    }

    consume(TokenType::right_brace, "expected '}' after contract methods");

    Statement statement{StatementKind::contract_statement, name.lexeme, nullptr, {}, {}};
    statement.body = std::move(methods);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::import_statement() {
    const Token& keyword = previous();
    const Token name = consume_identifier_like("expected module name after import");
    match(TokenType::semicolon);

    Statement statement{StatementKind::import_statement, name.lexeme, nullptr, {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::if_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before if body");
    std::vector<Statement> then_body = block();
    std::vector<Statement> else_body;

    if (match(TokenType::else_keyword)) {
        consume(TokenType::left_brace, "expected '{' before else body");
        else_body = block();
    }

    Statement statement{StatementKind::if_statement, "", std::move(condition), std::move(then_body),
                        std::move(else_body)};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::binding_statement() {
    const Token& name = consume_identifier_like("expected binding name");
    TypeAnnotation declared_type = optional_type_annotation();
    consume(TokenType::equal, "expected '=' after binding name");
    std::unique_ptr<Expression> value = expression();
    if (!match(TokenType::semicolon) && !check(TokenType::right_brace) && !check(TokenType::eof)) {
        throw std::runtime_error("expected ';' after binding statement");
    }

    Statement statement{StatementKind::binding, name.lexeme, std::move(value), {}, {}};
    statement.type = declared_type;
    statement.location = location_from_token(name);
    return statement;
}

Statement Parser::print_statement() {
    const Token& keyword = previous();
    consume(TokenType::left_paren, "expected '(' after print");
    std::unique_ptr<Expression> value = expression();
    std::vector<std::unique_ptr<Expression>> arguments;
    while (match(TokenType::comma)) {
        arguments.push_back(expression());
    }
    consume(TokenType::right_paren, "expected ')' after print expression");
    consume(TokenType::semicolon, "expected ';' after print statement");

    Statement statement{StatementKind::print, "", std::move(value), {}, {}};
    statement.arguments = std::move(arguments);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::return_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> value;
    if (!check(TokenType::semicolon)) {
        value = expression();
    }
    consume(TokenType::semicolon, "expected ';' after return value");

    Statement statement{StatementKind::return_statement, "", std::move(value), {}, {}};
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::struct_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected record name after record");
    std::vector<GenericParameter> parsed_generics;
    if (match(TokenType::less)) {
        parsed_generics = generic_parameters();
        consume(TokenType::greater, "expected '>' after record generic parameters");
    }

    std::vector<Type> contracts;
    if (match(TokenType::with_keyword)) {
        while (true) {
            contracts.push_back(make_generic_type(qualified_name()));
            if (!match(TokenType::comma)) {
                break;
            }
        }
    }

    std::vector<std::string> derives;
    if (match(TokenType::derive_keyword)) {
        while (true) {
            const Token& derive_name = consume(TokenType::identifier, "expected derive name after 'derive'");
            if (derive_name.lexeme != "eq" && derive_name.lexeme != "copy" && derive_name.lexeme != "debug") {
                throw std::runtime_error("unknown derive '" + derive_name.lexeme + "' (expected eq, copy, or debug)");
            }

            derives.push_back(derive_name.lexeme);
            if (!match(TokenType::comma)) {
                break;
            }
        }
    }

    consume(TokenType::left_brace, "expected '{' before record fields");

    std::vector<Parameter> fields;
    std::vector<Statement> methods;
    while (!check(TokenType::right_brace) && !is_at_end()) {
        const bool member_exported = match(TokenType::export_keyword);
        const bool member_static = match(TokenType::static_keyword);
        if (match(TokenType::fn_keyword)) {
            Statement method = function_statement();
            method.exported = member_exported;
            method.is_static_record_member = member_static;
            methods.push_back(std::move(method));
            match(TokenType::comma);
            continue;
        }

        if (member_static) {
            throw std::runtime_error("expected static record function");
        }

        const Token& field = consume_identifier_like("expected record field name");
        consume(TokenType::colon, "expected ':' after record field name");
        TypeAnnotation field_type = type_annotation();
        std::shared_ptr<Expression> default_value;
        if (match(TokenType::equal)) {
            default_value = expression();
        }
        fields.push_back(
            Parameter{field.lexeme, std::move(field_type), location_from_token(field), member_exported, default_value});

        if (check(TokenType::right_brace)) {
            break;
        }

        match(TokenType::comma);
    }

    consume(TokenType::right_brace, "expected '}' after record fields");

    const SourceLocation record_location = location_from_token(name);
    for (const std::string& derive : derives) {
        if (derive == "eq" && !record_defines_method(methods, "equals")) {
            methods.push_back(make_derived_equals(name.lexeme, parsed_generics, fields, record_location));
        } else if (derive == "copy" && !record_defines_method(methods, "copy")) {
            methods.push_back(make_derived_copy(name.lexeme, parsed_generics, fields, record_location));
        } else if (derive == "debug" && !record_defines_method(methods, "to_text")) {
            methods.push_back(make_derived_to_text(name.lexeme, fields, record_location));
        }
    }

    Statement statement{StatementKind::struct_statement, name.lexeme, nullptr, {}, {}};
    statement.body = std::move(methods);
    statement.parameters = std::move(fields);
    statement.generic_parameters = std::move(parsed_generics);
    statement.contracts = std::move(contracts);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::type_alias_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected type alias name after type");
    if (check(TokenType::less)) {
        throw std::runtime_error("generic type aliases are not supported yet");
    }

    consume(TokenType::equal, "expected '=' after type alias name");
    TypeAnnotation target_type = type_annotation();
    consume(TokenType::semicolon, "expected ';' after type alias");

    Statement statement{StatementKind::type_alias_statement, name.lexeme, nullptr, {}, {}};
    statement.type = std::move(target_type);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::enum_statement() {
    const Token& keyword = previous();
    const Token& name = consume(TokenType::identifier, "expected choice name after choice");
    std::vector<GenericParameter> parsed_generics;
    if (match(TokenType::less)) {
        parsed_generics = generic_parameters();
        consume(TokenType::greater, "expected '>' after choice generic parameters");
    }

    consume(TokenType::left_brace, "expected '{' before choice variants");

    std::vector<Parameter> variants;
    if (!check(TokenType::right_brace)) {
        while (true) {
            const Token& variant = consume(TokenType::identifier, "expected choice variant name");
            TypeAnnotation payload_type;
            if (match(TokenType::left_paren)) {
                payload_type = type_annotation();
                consume(TokenType::right_paren, "expected ')' after choice variant payload type");
            }

            variants.push_back(Parameter{variant.lexeme, std::move(payload_type), location_from_token(variant)});

            if (!match(TokenType::comma)) {
                break;
            }

            if (check(TokenType::right_brace)) {
                break;
            }
        }
    }

    consume(TokenType::right_brace, "expected '}' after choice variants");

    Statement statement{StatementKind::enum_statement, name.lexeme, nullptr, {}, {}};
    statement.parameters = std::move(variants);
    statement.generic_parameters = std::move(parsed_generics);
    statement.location = location_from_token(keyword);
    return statement;
}

Statement Parser::while_statement() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> condition = expression();
    consume(TokenType::left_brace, "expected '{' before while body");
    Statement statement{StatementKind::while_statement, "", std::move(condition), block(), {}};
    statement.location = location_from_token(keyword);
    return statement;
}

std::vector<Statement> Parser::block() {
    std::vector<Statement> statements;

    while (!check(TokenType::right_brace) && !is_at_end()) {
        statements.push_back(statement());
    }

    consume(TokenType::right_brace, "expected '}' after block");
    return statements;
}

std::string Parser::qualified_name() {
    if (!check_identifier_like() && !check(TokenType::int_keyword) && !check(TokenType::real_keyword)) {
        throw std::runtime_error("expected name");
    }

    std::string name = advance().lexeme;
    while (match(TokenType::dot)) {
        const Token member = consume_identifier_like("expected name after '.'");
        name += "." + member.lexeme;
    }

    return name;
}

std::vector<Parameter> Parser::parameters() {
    std::vector<Parameter> parsed_parameters;

    if (check(TokenType::right_paren)) {
        return parsed_parameters;
    }

    while (true) {
        const Token& name = consume(TokenType::identifier, "expected parameter name");
        parsed_parameters.push_back(Parameter{name.lexeme, optional_type_annotation(), location_from_token(name)});

        if (!match(TokenType::comma)) {
            break;
        }
    }

    return parsed_parameters;
}

std::vector<GenericParameter> Parser::generic_parameters() {
    std::vector<GenericParameter> parsed_parameters;

    while (true) {
        const Token& name = consume(TokenType::identifier, "expected generic parameter name");
        std::string bound;
        if (match(TokenType::is_keyword)) {
            bound = qualified_name();
        }

        parsed_parameters.push_back(GenericParameter{name.lexeme, std::move(bound), location_from_token(name)});

        if (!match(TokenType::comma)) {
            break;
        }
    }

    return parsed_parameters;
}

std::vector<std::unique_ptr<Expression>> Parser::arguments() {
    std::vector<std::unique_ptr<Expression>> parsed_arguments;

    if (check(TokenType::right_paren)) {
        return parsed_arguments;
    }

    while (true) {
        parsed_arguments.push_back(expression());

        if (!match(TokenType::comma)) {
            break;
        }
    }

    return parsed_arguments;
}

TypeAnnotation Parser::optional_type_annotation() {
    if (!match(TokenType::colon)) {
        return TypeAnnotation{};
    }

    return type_annotation();
}

TypeAnnotation Parser::type_annotation() {
    if (match(TokenType::int_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::int_type)};
    }

    if (match(TokenType::bool_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::bool_type)};
    }

    if (match(TokenType::i8_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::i8_type)};
    }

    if (match(TokenType::i16_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::i16_type)};
    }

    if (match(TokenType::i32_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::i32_type)};
    }

    if (match(TokenType::i64_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::i64_type)};
    }

    if (match(TokenType::isize_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::isize_type)};
    }

    if (match(TokenType::u8_keyword) || match(TokenType::uint8_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::u8_type)};
    }

    if (match(TokenType::u16_keyword) || match(TokenType::uint16_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::u16_type)};
    }

    if (match(TokenType::u32_keyword) || match(TokenType::uint32_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::u32_type)};
    }

    if (match(TokenType::u64_keyword) || match(TokenType::uint64_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::u64_type)};
    }

    if (match(TokenType::usize_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::usize_type)};
    }

    if (match(TokenType::real32_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::real32_type)};
    }

    if (match(TokenType::real64_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::real_type)};
    }

    if (match(TokenType::real_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::real_type)};
    }

    if (match(TokenType::glyph_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::glyph_type)};
    }

    if (match(TokenType::text_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::text_type)};
    }

    if (match(TokenType::unit_keyword)) {
        return TypeAnnotation{true, make_type(ValueType::unit_type)};
    }

    if (match(TokenType::left_bracket)) {
        TypeAnnotation element = type_annotation();
        consume(TokenType::right_bracket, "expected ']' after array element type");
        return TypeAnnotation{true, make_array_type(std::move(element.type))};
    }

    if (match(TokenType::left_paren)) {
        std::vector<Type> elements;
        elements.push_back(type_annotation().type);
        consume(TokenType::comma, "expected ',' in tuple type");
        if (check(TokenType::right_paren)) {
            throw std::runtime_error("tuple type needs at least two elements");
        }
        do {
            elements.push_back(type_annotation().type);
        } while (match(TokenType::comma) && !check(TokenType::right_paren));
        consume(TokenType::right_paren, "expected ')' after tuple type");
        return TypeAnnotation{true, make_tuple_type(std::move(elements))};
    }

    if (match(TokenType::identifier)) {
        std::string name = previous().lexeme;
        while (match(TokenType::dot)) {
            const Token member = consume_identifier_like("expected type name after '.'");
            name += "." + member.lexeme;
        }

        std::vector<Type> arguments;
        if (match(TokenType::less)) {
            while (true) {
                TypeAnnotation argument = type_annotation();
                arguments.push_back(std::move(argument.type));

                if (!match(TokenType::comma)) {
                    break;
                }
            }
            consume(TokenType::greater, "expected '>' after type arguments");
        }

        return TypeAnnotation{true, with_type_arguments(make_generic_type(std::move(name)), std::move(arguments))};
    }

    throw std::runtime_error("expected type annotation");
}

std::unique_ptr<Expression> Parser::assignment_target() {
    const Token name = consume_identifier_like("expected assignment target");
    std::unique_ptr<Expression> target = make_leaf(ExpressionKind::identifier, name.lexeme, location_from_token(name));

    while (true) {
        if (match(TokenType::dot)) {
            const SourceLocation location = target->location;
            const Token member = consume_identifier_like("expected member name after '.'");
            target = make_member(std::move(target), member.lexeme, location);
            continue;
        }

        if (match(TokenType::left_bracket)) {
            const SourceLocation location = target->location;
            std::unique_ptr<Expression> index = expression();
            consume(TokenType::right_bracket, "expected ']' after assignment target index");
            target = make_index(std::move(target), std::move(index), location);
            continue;
        }

        break;
    }

    return target;
}

std::unique_ptr<Expression> Parser::expression() {
    return range();
}

std::unique_ptr<Expression> Parser::range() {
    std::unique_ptr<Expression> expr = logical_or();

    if (match(TokenType::dot_dot)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = logical_or();
        expr = make_range(std::move(expr), std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::logical_or() {
    std::unique_ptr<Expression> expr = logical_and();

    while (match(TokenType::pipe_pipe)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = logical_and();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::logical_and() {
    std::unique_ptr<Expression> expr = equality();

    while (match(TokenType::amp_amp)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = equality();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::equality() {
    std::unique_ptr<Expression> expr = membership();

    while (match(TokenType::equal_equal) || match(TokenType::bang_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = membership();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::membership() {
    std::unique_ptr<Expression> expr = comparison();

    while (match(TokenType::in_keyword)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = comparison();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::comparison() {
    std::unique_ptr<Expression> expr = term();

    while (match(TokenType::greater) || match(TokenType::greater_equal) || match(TokenType::less) ||
           match(TokenType::less_equal)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = term();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::term() {
    std::unique_ptr<Expression> expr = factor();

    while (match(TokenType::plus) || match(TokenType::minus)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = factor();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::factor() {
    std::unique_ptr<Expression> expr = cast();

    while (match(TokenType::star) || match(TokenType::slash) || match(TokenType::percent)) {
        const Token& op = previous();
        std::unique_ptr<Expression> right = cast();
        expr = make_binary(std::move(expr), op.lexeme, std::move(right), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::cast() {
    std::unique_ptr<Expression> expr = unary();

    while (match(TokenType::to_keyword)) {
        const Token& op = previous();
        expr = make_cast(std::move(expr), type_annotation(), location_from_token(op));
    }

    return expr;
}

std::unique_ptr<Expression> Parser::unary() {
    if (match(TokenType::bang) || match(TokenType::minus)) {
        const Token& op = previous();
        return make_unary(op.lexeme, unary(), location_from_token(op));
    }

    if (match(TokenType::when_keyword)) {
        return when_expression();
    }

    return call();
}

std::unique_ptr<Expression> Parser::when_expression() {
    const Token& keyword = previous();
    std::unique_ptr<Expression> subject = expression();
    consume(TokenType::left_brace, "expected '{' before when arms");

    std::vector<std::unique_ptr<Expression>> cases;
    while (!check(TokenType::right_brace) && !is_at_end()) {
        std::unique_ptr<Expression> pattern;
        const bool legacy_arm = match(TokenType::is_keyword);
        pattern = when_pattern(!legacy_arm);

        cases.push_back(std::move(pattern));

        if (legacy_arm) {
            consume(TokenType::left_brace, "expected '{' before when arm body");
            cases.push_back(expression());
            match(TokenType::semicolon);
            consume(TokenType::right_brace, "expected '}' after when arm body");

            if (match(TokenType::comma)) {
                continue;
            }

            continue;
        }

        consume(TokenType::fat_arrow, "expected '=>' after when pattern");
        if (match(TokenType::left_brace)) {
            cases.push_back(expression());
            match(TokenType::semicolon);
            consume(TokenType::right_brace, "expected '}' after when arm body");
            match(TokenType::semicolon);
        } else {
            cases.push_back(expression());
            if (!match(TokenType::semicolon) && !match(TokenType::comma) && !check(TokenType::right_brace)) {
                throw std::runtime_error("expected ';' or ',' after when arm");
            }
        }

        if (match(TokenType::comma)) {
            continue;
        }
    }

    consume(TokenType::right_brace, "expected '}' after when arms");
    return make_match(std::move(subject), std::move(cases), location_from_token(keyword));
}

std::unique_ptr<Expression> Parser::when_pattern(bool allow_record_pattern) {
    if (check(TokenType::identifier) && peek().lexeme == "_") {
        const Token& wildcard = advance();
        return make_leaf(ExpressionKind::identifier, "_", location_from_token(wildcard));
    }

    std::unique_ptr<Expression> pattern = expression();
    if (allow_record_pattern && can_start_struct_literal(*pattern) && check(TokenType::left_brace)) {
        const SourceLocation location = pattern->location;
        const std::string name = expression_to_type_name(*pattern);
        consume(TokenType::left_brace, "expected '{' before record pattern fields");

        std::vector<std::string> field_names;
        std::vector<std::unique_ptr<Expression>> bindings;
        if (!check(TokenType::right_brace)) {
            while (true) {
                const Token field = consume(TokenType::identifier, "expected record pattern field name");
                field_names.push_back(field.lexeme);
                if (match(TokenType::colon)) {
                    if (check(TokenType::identifier) && peek().lexeme == "_") {
                        const Token& wildcard = advance();
                        bindings.push_back(make_leaf(ExpressionKind::identifier, "_", location_from_token(wildcard)));
                    } else {
                        const Token binding = consume(TokenType::identifier, "expected binding name in record pattern");
                        bindings.push_back(
                            make_leaf(ExpressionKind::identifier, binding.lexeme, location_from_token(binding)));
                    }
                } else {
                    bindings.push_back(make_leaf(ExpressionKind::identifier, field.lexeme, location_from_token(field)));
                }

                if (!match(TokenType::comma)) {
                    break;
                }

                if (check(TokenType::right_brace)) {
                    break;
                }
            }
        }

        consume(TokenType::right_brace, "expected '}' after record pattern fields");
        return make_struct_literal(name, std::move(field_names), std::move(bindings), location);
    }

    return pattern;
}

std::unique_ptr<Expression> Parser::call() {
    std::unique_ptr<Expression> expr = primary();

    while (true) {
        if (match(TokenType::left_paren)) {
            const SourceLocation location = expr->location;
            if (expr->kind != ExpressionKind::identifier) {
                throw std::runtime_error("expected function name before arguments");
            }

            std::vector<std::unique_ptr<Expression>> parsed_arguments = arguments();
            consume(TokenType::right_paren, "expected ')' after function arguments");
            expr = make_call(expr->lexeme, std::move(parsed_arguments), location);
            continue;
        }

        if (match(TokenType::dot)) {
            const SourceLocation location = expr->location;
            const Token member = consume_identifier_like("expected member name after '.'");
            if (match(TokenType::left_paren)) {
                std::vector<std::unique_ptr<Expression>> parsed_arguments = arguments();
                consume(TokenType::right_paren, "expected ')' after module function arguments");
                expr = make_method_call(std::move(expr), member.lexeme, std::move(parsed_arguments), location);
            } else {
                expr = make_member(std::move(expr), member.lexeme, location);
            }
            continue;
        }

        if (can_start_struct_literal(*expr) && looks_like_struct_literal()) {
            const SourceLocation location = expr->location;
            const std::string name = expression_to_type_name(*expr);
            consume(TokenType::left_brace, "expected '{' before record literal fields");

            std::vector<std::string> field_names;
            std::vector<std::unique_ptr<Expression>> values;
            if (!check(TokenType::right_brace)) {
                while (true) {
                    const Token field = consume(TokenType::identifier, "expected record literal field name");
                    consume(TokenType::colon, "expected ':' after record literal field name");
                    field_names.push_back(field.lexeme);
                    values.push_back(expression());

                    if (!match(TokenType::comma)) {
                        break;
                    }

                    if (check(TokenType::right_brace)) {
                        break;
                    }
                }
            }

            consume(TokenType::right_brace, "expected '}' after record literal fields");
            expr = make_struct_literal(name, std::move(field_names), std::move(values), location);
            continue;
        }

        if (match(TokenType::left_bracket)) {
            const SourceLocation location = expr->location;
            std::unique_ptr<Expression> start;
            if (!check(TokenType::colon)) {
                start = expression();
            }

            if (match(TokenType::colon)) {
                std::unique_ptr<Expression> end;
                if (!check(TokenType::right_bracket)) {
                    end = expression();
                }
                consume(TokenType::right_bracket, "expected ']' after slice");
                expr = make_slice(std::move(expr), std::move(start), std::move(end), location);
                continue;
            }

            if (start == nullptr) {
                throw std::runtime_error("expected index expression");
            }

            consume(TokenType::right_bracket, "expected ']' after array index");
            expr = make_index(std::move(expr), std::move(start), location);
            continue;
        }

        if (match(TokenType::question)) {
            const SourceLocation location = expr->location;
            expr = make_try(std::move(expr), location);
            continue;
        }

        break;
    }

    return expr;
}

std::unique_ptr<Expression> Parser::primary() {
    if (match(TokenType::number)) {
        return make_leaf(ExpressionKind::number, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::float_number)) {
        return make_leaf(ExpressionKind::floating, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::char_literal)) {
        return make_leaf(ExpressionKind::character, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::string_literal)) {
        return make_leaf(ExpressionKind::string, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::true_keyword) || match(TokenType::false_keyword)) {
        return make_leaf(ExpressionKind::boolean, previous().lexeme, location_from_token(previous()));
    }

    if (match_identifier_like()) {
        return make_leaf(ExpressionKind::identifier, previous().lexeme, location_from_token(previous()));
    }

    if (match(TokenType::left_bracket)) {
        const SourceLocation location = location_from_token(previous());
        std::vector<std::unique_ptr<Expression>> elements;
        if (!check(TokenType::right_bracket)) {
            elements.push_back(expression());

            if (match(TokenType::for_keyword)) {
                const Token variable = consume_identifier_like("expected comprehension variable after 'for'");
                consume(TokenType::in_keyword, "expected 'in' after comprehension variable");
                std::unique_ptr<Expression> iterable = expression();
                std::unique_ptr<Expression> condition;
                if (match(TokenType::if_keyword)) {
                    condition = expression();
                }

                consume(TokenType::right_bracket, "expected ']' after array comprehension");
                return make_array_comprehension(std::move(elements.front()), variable.lexeme, std::move(iterable),
                                                std::move(condition), location);
            }

            while (match(TokenType::comma)) {
                elements.push_back(expression());
            }
        }

        consume(TokenType::right_bracket, "expected ']' after array literal");
        return make_array(std::move(elements), location);
    }

    if (match(TokenType::left_paren)) {
        const SourceLocation location = location_from_token(previous());
        std::unique_ptr<Expression> expr = expression();
        if (match(TokenType::comma)) {
            std::vector<std::unique_ptr<Expression>> elements;
            elements.push_back(std::move(expr));
            if (check(TokenType::right_paren)) {
                throw std::runtime_error("tuple literal needs at least two elements");
            }
            do {
                elements.push_back(expression());
            } while (match(TokenType::comma) && !check(TokenType::right_paren));

            consume(TokenType::right_paren, "expected ')' after tuple literal");
            return make_tuple(std::move(elements), location);
        }

        consume(TokenType::right_paren, "expected ')' after expression");
        return expr;
    }

    throw std::runtime_error("expected expression");
}

} // namespace dune
