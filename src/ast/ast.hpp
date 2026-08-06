#pragma once

#include "diagnostics/source_location.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dune {

struct Expression;

enum class ValueType {
    int_type,
    bool_type,
    i8_type,
    i16_type,
    i32_type,
    i64_type,
    isize_type,
    u8_type,
    u16_type,
    u32_type,
    u64_type,
    usize_type,
    real32_type,
    real_type,
    glyph_type,
    text_type,
    unit_type,
    array_type,
    tuple_type,
    generic_type,
    struct_type,
    enum_type,
    function_type,
    // A compile-time integer that appears as a generic argument, e.g. the `3`s in
    // `Matrix<real64, 3, 3>`. It is not a value type in its own right; it only ever
    // lives inside another type's `arguments` list to carry a static shape.
    const_int_type,
};

// For a `function_type`, `arguments` holds the parameter types and `element`
// holds the return type (unit when the function returns nothing). For a
// `const_int_type`, `const_value` holds the literal. All other kinds leave those
// fields to their usual meaning.
struct Type {
    ValueType kind = ValueType::int_type;
    std::shared_ptr<Type> element;
    std::string name;
    std::vector<Type> arguments;
    // Only meaningful when `kind == const_int_type`: the const generic argument's value.
    long long const_value = 0;
};

struct TypeAnnotation {
    bool has_type = false;
    Type type;
};

struct Parameter {
    std::string name;
    TypeAnnotation type;
    SourceLocation location;
    bool exported = false;
    std::shared_ptr<Expression> default_value;
    // Doc-comment written above a record field (markers stripped), or empty.
    // Function parameters and choice variants leave this unset.
    std::string doc_comment;
};

struct GenericParameter {
    std::string name;
    // Zero or more constraints on this type parameter, e.g. `T is ordered + Display`
    // parses to {"ordered", "Display"}. An empty list means an unbounded parameter.
    std::vector<std::string> bounds;
    SourceLocation location;
};

enum class ExpressionKind {
    identifier,
    number,
    floating,
    character,
    string,
    boolean,
    array,
    array_comprehension,
    tuple,
    struct_literal,
    index,
    slice,
    member,
    unary,
    try_expression,
    cast,
    binary,
    range,
    when_expression,
    call,
    method_call,
};

struct Expression {
    ExpressionKind kind;
    std::string lexeme;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    std::vector<std::unique_ptr<Expression>> arguments;
    std::vector<std::string> field_names;
    SourceLocation location;
    TypeAnnotation type;
};

enum class StatementKind {
    binding,
    const_statement,
    assign,
    block,
    if_statement,
    while_statement,
    for_statement,
    for_in_statement,
    break_statement,
    continue_statement,
    function,
    method_block,
    struct_statement,
    enum_statement,
    contract_statement,
    type_alias_statement,
    return_statement,
    expression_statement,
    import_statement,
    module_declaration,
    test_block,
};

struct Statement {
    StatementKind kind;
    std::string name;
    std::unique_ptr<Expression> expression;
    std::vector<Statement> body;
    std::vector<Statement> else_body;
    TypeAnnotation type;
    std::vector<Parameter> parameters;
    std::vector<GenericParameter> generic_parameters;
    std::vector<Type> contracts;
    SourceLocation location;
    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Statement> increment;
    bool exported = false;
    bool is_extern = false;
    bool is_record_member = false;
    // Set by receiver-method desugaring when an implicit `this` parameter is
    // inserted. Kept separate from is_record_member because extension methods
    // also have receivers but use different export/resolution rules.
    bool has_receiver = false;
    bool is_constructor = false;
    bool is_static_record_member = false;
    bool is_foreknown = false;
    std::string extern_symbol;
    std::string owner_record;
    std::unique_ptr<Expression> target;
    std::vector<std::unique_ptr<Expression>> arguments;
    // Modules v2 import metadata (consumed by the module loader, then cleared):
    // `import math as m;` sets module_alias="m"; `from matrix import A, B;` fills
    // import_symbols with the selectively imported names.
    std::string module_alias;
    std::vector<std::string> import_symbols;
    // The doc-comment block written directly above this declaration (markers
    // stripped, lines joined by '\n'), or empty. Populated by the parser from the
    // leading comment of the statement's first token; rendered by LSP hover and
    // available to documentation tooling.
    std::string doc_comment;
};

struct Program {
    std::vector<Statement> statements;
};

} // namespace dune
