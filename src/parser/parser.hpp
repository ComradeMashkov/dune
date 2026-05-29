#pragma once

#include "ast/ast.hpp"
#include "lexer/lexer.hpp"

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace dune {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    Program parse();

private:
    bool is_at_end() const;
    bool check(TokenType type) const;
    bool check_next(TokenType type) const;
    bool check_identifier_like() const;
    bool match(TokenType type);
    bool looks_like_struct_literal() const;
    bool looks_like_assignment_statement() const;
    bool looks_like_tuple_assignment_statement() const;
    bool looks_like_binding_declaration() const;

    const Token& advance();
    const Token& peek() const;
    const Token& previous() const;
    const Token& consume(TokenType type, std::string_view message);
    Token consume_identifier_like(std::string_view message);
    bool match_identifier_like();

    Statement statement();
    Statement assignment_statement(bool require_semicolon = true);
    Statement tuple_assignment_statement();
    Statement block_statement();
    Statement break_statement();
    Statement continue_statement();
    Statement contract_statement();
    Statement const_statement();
    Statement export_statement();
    Statement extern_statement();
    Statement for_statement();
    Statement for_in_statement(const Token& keyword);
    Statement function_statement(bool is_extern = false);
    Statement finish_function_statement(const Token& name, std::vector<GenericParameter> leading_generics = {},
                                        bool is_extern = false);
    Statement import_statement();
    Statement enum_statement();
    Statement if_statement();
    Statement binding_statement();
    Statement method_statement();
    Statement print_statement();
    Statement return_statement();
    Statement struct_statement();
    Statement while_statement();

    std::vector<Statement> block();
    Statement contract_method_signature();
    std::string qualified_name();
    std::vector<Parameter> parameters();
    std::vector<GenericParameter> generic_parameters();
    std::vector<std::unique_ptr<Expression>> arguments();
    TypeAnnotation optional_type_annotation();
    TypeAnnotation type_annotation();

    std::unique_ptr<Expression> assignment_target();
    std::unique_ptr<Expression> expression();
    std::unique_ptr<Expression> range();
    std::unique_ptr<Expression> logical_or();
    std::unique_ptr<Expression> logical_and();
    std::unique_ptr<Expression> equality();
    std::unique_ptr<Expression> membership();
    std::unique_ptr<Expression> comparison();
    std::unique_ptr<Expression> term();
    std::unique_ptr<Expression> factor();
    std::unique_ptr<Expression> cast();
    std::unique_ptr<Expression> unary();
    std::unique_ptr<Expression> call();
    std::unique_ptr<Expression> when_expression();
    std::unique_ptr<Expression> when_pattern(bool allow_record_pattern);
    std::unique_ptr<Expression> primary();

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};

} // namespace dune
