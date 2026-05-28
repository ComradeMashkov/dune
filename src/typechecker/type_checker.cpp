#include "type_checker.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

TypeAnnotation expected_type(Type type) {
    return TypeAnnotation{true, std::move(type)};
}

std::string base_name(const std::string& name) {
    const std::size_t separator = name.rfind('.');
    return separator == std::string::npos ? name : name.substr(separator + 1);
}

Type clone_type(const Type& type) {
    Type result{type.kind, nullptr};
    result.name = type.name;
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

TypeAnnotation clone_type_annotation(const TypeAnnotation& annotation) {
    if (!annotation.has_type) {
        return {};
    }

    return TypeAnnotation{true, clone_type(annotation.type)};
}

TypeAnnotation substitute_type_annotation(const TypeAnnotation& annotation,
                                          const std::unordered_map<std::string, Type>& substitutions) {
    if (!annotation.has_type) {
        return {};
    }

    return TypeAnnotation{true, substitute_type(annotation.type, substitutions)};
}

std::unique_ptr<Expression> clone_expression(const Expression& expression);

std::unique_ptr<Expression> clone_expression_pointer(const std::unique_ptr<Expression>& expression) {
    if (expression == nullptr) {
        return nullptr;
    }

    return clone_expression(*expression);
}

std::unique_ptr<Expression> clone_expression(const Expression& expression) {
    auto result = std::make_unique<Expression>(Expression{expression.kind, expression.lexeme,
                                                          clone_expression_pointer(expression.left),
                                                          clone_expression_pointer(expression.right)});
    result->location = expression.location;
    result->type = clone_type_annotation(expression.type);
    result->field_names = expression.field_names;
    result->arguments.reserve(expression.arguments.size());
    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        result->arguments.push_back(clone_expression_pointer(argument));
    }

    return result;
}

Statement clone_statement(const Statement& statement);

std::unique_ptr<Statement> clone_statement_pointer(const std::unique_ptr<Statement>& statement) {
    if (statement == nullptr) {
        return nullptr;
    }

    return std::make_unique<Statement>(clone_statement(*statement));
}

Statement clone_statement(const Statement& statement) {
    Statement result{statement.kind, statement.name, clone_expression_pointer(statement.expression), {}, {}};
    result.body.reserve(statement.body.size());
    for (const Statement& child : statement.body) {
        result.body.push_back(clone_statement(child));
    }

    result.else_body.reserve(statement.else_body.size());
    for (const Statement& child : statement.else_body) {
        result.else_body.push_back(clone_statement(child));
    }

    result.type = clone_type_annotation(statement.type);
    result.parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        result.parameters.push_back(
            Parameter{parameter.name, clone_type_annotation(parameter.type), parameter.location});
    }

    result.generic_parameters = statement.generic_parameters;
    result.location = statement.location;
    result.initializer = clone_statement_pointer(statement.initializer);
    result.increment = clone_statement_pointer(statement.increment);
    result.exported = statement.exported;
    result.is_extern = statement.is_extern;
    result.extern_symbol = statement.extern_symbol;
    result.target = clone_expression_pointer(statement.target);
    return result;
}

void substitute_expression(Expression& expression, const std::unordered_map<std::string, Type>& substitutions);

void substitute_statement(Statement& statement, const std::unordered_map<std::string, Type>& substitutions) {
    statement.type = substitute_type_annotation(statement.type, substitutions);
    for (Parameter& parameter : statement.parameters) {
        parameter.type = substitute_type_annotation(parameter.type, substitutions);
    }

    if (statement.expression != nullptr) {
        substitute_expression(*statement.expression, substitutions);
    }

    if (statement.target != nullptr) {
        substitute_expression(*statement.target, substitutions);
    }

    for (Statement& child : statement.body) {
        substitute_statement(child, substitutions);
    }

    for (Statement& child : statement.else_body) {
        substitute_statement(child, substitutions);
    }

    if (statement.initializer != nullptr) {
        substitute_statement(*statement.initializer, substitutions);
    }

    if (statement.increment != nullptr) {
        substitute_statement(*statement.increment, substitutions);
    }
}

void substitute_expression(Expression& expression, const std::unordered_map<std::string, Type>& substitutions) {
    expression.type = substitute_type_annotation(expression.type, substitutions);

    if (expression.left != nullptr) {
        substitute_expression(*expression.left, substitutions);
    }

    if (expression.right != nullptr) {
        substitute_expression(*expression.right, substitutions);
    }

    for (std::unique_ptr<Expression>& argument : expression.arguments) {
        if (argument != nullptr) {
            substitute_expression(*argument, substitutions);
        }
    }
}

std::vector<const Expression*> expression_refs(const std::vector<std::unique_ptr<Expression>>& arguments) {
    std::vector<const Expression*> result;
    result.reserve(arguments.size());
    for (const std::unique_ptr<Expression>& argument : arguments) {
        result.push_back(argument.get());
    }

    return result;
}

const std::string* root_identifier(const Expression& expression) {
    if (expression.kind == ExpressionKind::identifier) {
        return &expression.lexeme;
    }

    if ((expression.kind == ExpressionKind::member || expression.kind == ExpressionKind::index) &&
        expression.left != nullptr) {
        return root_identifier(*expression.left);
    }

    return nullptr;
}

} // namespace

Type make_type(ValueType type) {
    return Type{type, nullptr};
}

Type make_array_type(Type element) {
    return Type{ValueType::array_type, std::make_shared<Type>(std::move(element))};
}

Type make_struct_type(std::string name) {
    Type type{ValueType::struct_type, nullptr};
    type.name = std::move(name);
    return type;
}

Type make_struct_type(std::string name, std::vector<Type> arguments) {
    Type type = make_struct_type(std::move(name));
    type.arguments = std::move(arguments);
    return type;
}

Type make_enum_type(std::string name) {
    Type type{ValueType::enum_type, nullptr};
    type.name = std::move(name);
    return type;
}

Type make_enum_type(std::string name, std::vector<Type> arguments) {
    Type type = make_enum_type(std::move(name));
    type.arguments = std::move(arguments);
    return type;
}

std::string type_name(ValueType type) {
    switch (type) {
    case ValueType::int_type:
        return "int";
    case ValueType::bool_type:
        return "bool";
    case ValueType::i8_type:
        return "i8";
    case ValueType::i16_type:
        return "i16";
    case ValueType::i32_type:
        return "i32";
    case ValueType::i64_type:
        return "i64";
    case ValueType::isize_type:
        return "isize";
    case ValueType::u8_type:
        return "u8";
    case ValueType::u16_type:
        return "u16";
    case ValueType::u32_type:
        return "u32";
    case ValueType::u64_type:
        return "u64";
    case ValueType::usize_type:
        return "usize";
    case ValueType::real32_type:
        return "real32";
    case ValueType::real_type:
        return "real";
    case ValueType::glyph_type:
        return "glyph";
    case ValueType::text_type:
        return "text";
    case ValueType::unit_type:
        return "unit";
    case ValueType::array_type:
        return "array";
    case ValueType::generic_type:
        return "generic";
    case ValueType::struct_type:
        return "record";
    case ValueType::enum_type:
        return "choice";
    }

    return "unknown";
}

std::string type_name(const Type& type) {
    if (type.kind == ValueType::array_type) {
        if (type.element == nullptr) {
            return "[unknown]";
        }

        return "[" + type_name(*type.element) + "]";
    }

    if (type.kind == ValueType::generic_type || type.kind == ValueType::struct_type ||
        type.kind == ValueType::enum_type) {
        if (!type.arguments.empty()) {
            std::string name = type.name + "<";
            for (std::size_t index = 0; index < type.arguments.size(); ++index) {
                if (index > 0) {
                    name += ", ";
                }
                name += type_name(type.arguments[index]);
            }
            name += ">";
            return name;
        }

        return type.name;
    }

    return type_name(type.kind);
}

std::string type_key(const Type& type) {
    if (type.kind == ValueType::array_type) {
        if (type.element == nullptr) {
            return "array_unknown";
        }

        return "array_" + type_key(*type.element);
    }

    if (type.kind == ValueType::generic_type) {
        if (!type.arguments.empty()) {
            std::string key = "generic_" + type.name;
            for (const Type& argument : type.arguments) {
                key += "_" + type_key(argument);
            }
            return key;
        }
        return "generic_" + type.name;
    }

    if (type.kind == ValueType::struct_type) {
        std::string key = "record_" + type.name;
        for (const Type& argument : type.arguments) {
            key += "_" + type_key(argument);
        }
        return key;
    }

    if (type.kind == ValueType::enum_type) {
        std::string key = "choice_" + type.name;
        for (const Type& argument : type.arguments) {
            key += "_" + type_key(argument);
        }
        return key;
    }

    return type_name(type.kind);
}

std::string function_key(const std::string& name, const std::vector<Type>& parameters) {
    std::string key = name + "__";
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (index > 0) {
            key += "_";
        }

        key += type_key(parameters[index]);
    }

    return key;
}

void TypeChecker::check(const Program& program) {
    structs_.clear();
    enums_.clear();
    functions_.clear();
    overloads_.clear();
    generic_overloads_.clear();
    imports_.clear();
    global_constants_.clear();
    module_exports_.clear();
    known_modules_.clear();
    expression_types_.clear();
    resolved_calls_.clear();
    resolved_variants_.clear();
    instantiated_function_keys_.clear();
    instantiated_functions_.clear();
    instantiated_function_traces_.clear();
    loop_depth_ = 0;

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::struct_statement) {
            declare_struct(statement);
        }

        if (statement.kind == StatementKind::enum_statement) {
            declare_enum(statement);
        }
    }

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::struct_statement) {
            define_struct(statement);
        }

        if (statement.kind == StatementKind::enum_statement) {
            define_enum(statement);
        }

        if (statement.kind == StatementKind::function) {
            if (statement.generic_parameters.empty()) {
                collect_function(statement);
            } else {
                collect_generic_function(statement);
            }
        }

        if (statement.kind == StatementKind::const_statement) {
            collect_known_module(statement.name);
        }

        collect_module_export(statement);
    }

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::import_statement) {
            if (!is_known_module(statement.name)) {
                throw std::runtime_error(diagnostic(statement.location, "unknown module '" + statement.name + "'"));
            }

            imports_.insert(statement.name);
        }
    }

    reset_scopes();
    current_function_ = nullptr;
    loop_depth_ = 0;
    for (const Statement& statement : program.statements) {
        if (statement.kind != StatementKind::function && statement.kind != StatementKind::import_statement &&
            statement.kind != StatementKind::struct_statement && statement.kind != StatementKind::enum_statement) {
            check_statement(statement);
        }
    }

    if (!scopes_.empty()) {
        for (const auto& [name, binding] : scopes_.front()) {
            if (binding.constant) {
                global_constants_.emplace(name, binding.type);
            }
        }
    }

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function && statement.generic_parameters.empty()) {
            check_function(statement);
        }
    }

    for (std::size_t index = 0; index < instantiated_functions_.size(); ++index) {
        const Statement& function = instantiated_functions_[index];
        std::vector<Type> parameters;
        parameters.reserve(function.parameters.size());
        for (const Parameter& parameter : function.parameters) {
            parameters.push_back(annotation_or_default(parameter.type));
        }
        const std::string key = function_key(function.name, parameters);
        const auto trace = instantiated_function_traces_.find(key);
        try {
            check_function(function);
        } catch (const std::runtime_error& error) {
            if (trace != instantiated_function_traces_.end()) {
                throw std::runtime_error(std::string(error.what()) + "\nwhile instantiating " + trace->second);
            }

            throw;
        }
    }
}

const std::unordered_map<const Expression*, Type>& TypeChecker::expression_types() const {
    return expression_types_;
}

const std::unordered_map<const Expression*, std::string>& TypeChecker::resolved_calls() const {
    return resolved_calls_;
}

const std::unordered_map<const Expression*, TypeChecker::VariantResolution>& TypeChecker::resolved_variants() const {
    return resolved_variants_;
}

const std::deque<Statement>& TypeChecker::instantiated_functions() const {
    return instantiated_functions_;
}

const std::unordered_map<std::string, TypeChecker::StructDefinition>& TypeChecker::structs() const {
    return structs_;
}

const std::unordered_map<std::string, TypeChecker::EnumDefinition>& TypeChecker::enums() const {
    return enums_;
}

void TypeChecker::declare_struct(const Statement& statement) {
    if (statement.name.empty()) {
        throw std::runtime_error(diagnostic(statement.location, "record needs a name"));
    }

    if (structs_.contains(statement.name)) {
        throw std::runtime_error(diagnostic(statement.location, "duplicate record '" + statement.name + "'"));
    }

    collect_known_module(statement.name);
    structs_.emplace(statement.name,
                     StructDefinition{statement.name, statement.generic_parameters, {}, {}, statement.location});
}

void TypeChecker::declare_enum(const Statement& statement) {
    if (statement.name.empty()) {
        throw std::runtime_error(diagnostic(statement.location, "choice needs a name"));
    }

    if (enums_.contains(statement.name)) {
        throw std::runtime_error(diagnostic(statement.location, "duplicate choice '" + statement.name + "'"));
    }

    collect_known_module(statement.name);
    enums_.emplace(statement.name,
                   EnumDefinition{statement.name, statement.generic_parameters, {}, {}, statement.location});
}

void TypeChecker::define_struct(const Statement& statement) {
    auto definition = structs_.find(statement.name);
    if (definition == structs_.end()) {
        throw std::runtime_error(diagnostic(statement.location, "undefined record '" + statement.name + "'"));
    }

    definition->second.fields.clear();
    definition->second.field_indices.clear();
    std::unordered_set<std::string> generic_names;
    for (const GenericParameter& parameter : statement.generic_parameters) {
        generic_names.insert(parameter.name);
    }

    for (const Parameter& field : statement.parameters) {
        if (definition->second.field_indices.contains(field.name)) {
            throw std::runtime_error(
                diagnostic(field.location, "duplicate field '" + field.name + "' in record '" + statement.name + "'"));
        }

        const Type field_type = annotation_or_default(field.type, generic_names);
        if (field_type.kind == ValueType::unit_type) {
            throw std::runtime_error(diagnostic(field.location, "field '" + field.name + "' cannot have type 'unit'"));
        }

        const std::size_t index = definition->second.fields.size();
        definition->second.field_indices.emplace(field.name, index);
        definition->second.fields.push_back(StructField{field.name, field_type, field.location});
    }
}

void TypeChecker::define_enum(const Statement& statement) {
    auto definition = enums_.find(statement.name);
    if (definition == enums_.end()) {
        throw std::runtime_error(diagnostic(statement.location, "undefined choice '" + statement.name + "'"));
    }

    definition->second.variants.clear();
    definition->second.variant_indices.clear();
    std::unordered_set<std::string> generic_names;
    for (const GenericParameter& parameter : statement.generic_parameters) {
        generic_names.insert(parameter.name);
    }

    for (const Parameter& variant : statement.parameters) {
        if (definition->second.variant_indices.contains(variant.name)) {
            throw std::runtime_error(diagnostic(variant.location, "duplicate variant '" + variant.name +
                                                                      "' in choice '" + statement.name + "'"));
        }

        Type payload_type = variant.type.has_type ? annotation_or_default(variant.type, generic_names)
                                                  : make_type(ValueType::unit_type);
        if (variant.type.has_type && payload_type.kind == ValueType::unit_type) {
            throw std::runtime_error(
                diagnostic(variant.location, "variant '" + variant.name + "' cannot carry type 'unit'"));
        }

        const std::size_t tag = definition->second.variants.size();
        definition->second.variant_indices.emplace(variant.name, tag);
        definition->second.variants.push_back(
            EnumVariant{variant.name, variant.type.has_type, std::move(payload_type), tag, variant.location});
    }
}

void TypeChecker::collect_function(const Statement& statement) {
    FunctionSignature signature;
    signature.name = statement.name;
    signature.return_type = annotation_or_default(statement.type);
    signature.location = statement.location;

    for (const Parameter& parameter : statement.parameters) {
        const Type parameter_type = annotation_or_default(parameter.type);
        if (parameter_type.kind == ValueType::unit_type) {
            throw std::runtime_error(
                diagnostic(parameter.location, "parameter '" + parameter.name + "' cannot have type 'unit'"));
        }

        signature.parameters.push_back(parameter_type);
    }

    signature.key = function_key(signature.name, signature.parameters);
    if (functions_.contains(signature.key)) {
        throw std::runtime_error(
            diagnostic(statement.location, "duplicate overload for function '" + statement.name + "'"));
    }

    collect_known_module(signature.name);
    overloads_[signature.name].push_back(signature.key);
    functions_.emplace(signature.key, std::move(signature));
}

void TypeChecker::collect_generic_function(const Statement& statement) {
    std::unordered_set<std::string> generic_names;
    for (const GenericParameter& parameter : statement.generic_parameters) {
        generic_names.insert(parameter.name);
    }

    FunctionSignature signature;
    signature.name = statement.name;
    signature.return_type = annotation_or_default(statement.type, generic_names);
    signature.location = statement.location;

    for (const Parameter& parameter : statement.parameters) {
        signature.parameters.push_back(annotation_or_default(parameter.type, generic_names));
    }

    signature.key = function_key(signature.name, signature.parameters);
    generic_overloads_[signature.name].push_back(&statement);
    collect_known_module(signature.name);
}

void TypeChecker::check_function(const Statement& statement) {
    std::vector<Type> parameters;
    parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameters.push_back(annotation_or_default(parameter.type));
    }

    const FunctionSignature& function =
        find_function_by_key(function_key(statement.name, parameters), statement.location);

    reset_scopes();
    if (statement.is_extern) {
        return;
    }

    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        const Parameter& parameter = statement.parameters[index];
        declare_binding(parameter.name, function.parameters[index], false, parameter.location);
    }

    current_function_ = &function;
    check_statements(statement.body);
    const bool has_tail_expression = !statement.body.empty() &&
                                     statement.body.back().kind == StatementKind::expression_statement &&
                                     statement.body.back().expression != nullptr;
    if (function.return_type.kind != ValueType::unit_type && has_tail_expression) {
        expect_type(function.return_type,
                    check_expression(*statement.body.back().expression, expected_type(function.return_type)),
                    statement.body.back().expression->location);
    }

    if (function.return_type.kind != ValueType::unit_type && !has_tail_expression &&
        !statements_return(statement.body)) {
        throw std::runtime_error(diagnostic(statement.location, "function '" + statement.name + "' must return type '" +
                                                                    type_name(function.return_type) + "'"));
    }

    current_function_ = nullptr;
}

void TypeChecker::check_statement(const Statement& statement) {
    switch (statement.kind) {
    case StatementKind::binding:
    case StatementKind::const_statement: {
        TypeAnnotation expected = statement.type;
        if (expected.has_type) {
            expected.type = normalize_type(expected.type);
        }
        Type actual = check_expression(*statement.expression, expected);
        if (!expected.has_type) {
            expected = expected_type(actual);
        }

        expect_type(expected.type, actual, statement.expression->location);
        if (expected.type.kind == ValueType::unit_type) {
            throw std::runtime_error(diagnostic(statement.location, "variables cannot have type 'unit'"));
        }

        declare_binding(statement.name, expected.type, statement.kind == StatementKind::const_statement,
                        statement.location);
        return;
    }
    case StatementKind::assign: {
        if (statement.target != nullptr && statement.target->kind != ExpressionKind::identifier) {
            const Type expected = check_assignment_target(*statement.target, statement.location);
            expect_type(expected, check_expression(*statement.expression, expected_type(expected)),
                        statement.expression->location);
            return;
        }

        const std::string name = statement.target != nullptr ? statement.target->lexeme : statement.name;
        VariableBinding* variable = find_binding(name);
        if (variable == nullptr) {
            if (global_constants_.contains(name)) {
                throw std::runtime_error(diagnostic(statement.location, "cannot assign to constant '" + name + "'"));
            }

            const Type actual = check_expression(*statement.expression);
            if (actual.kind == ValueType::unit_type) {
                throw std::runtime_error(diagnostic(statement.location, "variables cannot have type 'unit'"));
            }
            declare_binding(name, actual, false, statement.location);
            return;
        }
        if (variable->constant) {
            throw std::runtime_error(diagnostic(statement.location, "cannot assign to constant '" + name + "'"));
        }

        expect_type(variable->type, check_expression(*statement.expression, expected_type(variable->type)),
                    statement.expression->location);
        return;
    }
    case StatementKind::print: {
        const Type type = check_expression(*statement.expression);
        if (type.kind == ValueType::unit_type || type.kind == ValueType::array_type ||
            type.kind == ValueType::struct_type || type.kind == ValueType::enum_type) {
            throw std::runtime_error(
                diagnostic(statement.expression->location, "cannot print type '" + type_name(type) + "'"));
        }

        return;
    }
    case StatementKind::block:
        push_scope();
        check_statements(statement.body);
        pop_scope();
        return;
    case StatementKind::if_statement:
        expect_type(make_type(ValueType::bool_type), check_expression(*statement.expression),
                    statement.expression->location);
        push_scope();
        check_statements(statement.body);
        pop_scope();
        push_scope();
        check_statements(statement.else_body);
        pop_scope();
        return;
    case StatementKind::while_statement:
        expect_type(make_type(ValueType::bool_type), check_expression(*statement.expression),
                    statement.expression->location);
        ++loop_depth_;
        push_scope();
        check_statements(statement.body);
        pop_scope();
        --loop_depth_;
        return;
    case StatementKind::for_statement:
        push_scope();
        if (statement.initializer != nullptr) {
            check_statement(*statement.initializer);
        }

        expect_type(make_type(ValueType::bool_type), check_expression(*statement.expression),
                    statement.expression->location);
        ++loop_depth_;
        push_scope();
        check_statements(statement.body);
        pop_scope();
        --loop_depth_;
        if (statement.increment != nullptr) {
            check_statement(*statement.increment);
        }
        pop_scope();
        return;
    case StatementKind::break_statement:
        if (loop_depth_ == 0) {
            throw std::runtime_error(diagnostic(statement.location, "break statement outside loop"));
        }
        return;
    case StatementKind::continue_statement:
        if (loop_depth_ == 0) {
            throw std::runtime_error(diagnostic(statement.location, "continue statement outside loop"));
        }
        return;
    case StatementKind::function:
        throw std::runtime_error(diagnostic(statement.location, "function declarations are only allowed at top level"));
    case StatementKind::method_block:
        throw std::runtime_error(diagnostic(statement.location, "method declarations are only allowed at top level"));
    case StatementKind::struct_statement:
        throw std::runtime_error(diagnostic(statement.location, "record declarations are only allowed at top level"));
    case StatementKind::enum_statement:
        throw std::runtime_error(diagnostic(statement.location, "choice declarations are only allowed at top level"));
    case StatementKind::return_statement:
        if (current_function_ == nullptr) {
            throw std::runtime_error(diagnostic(statement.location, "return statement outside function"));
        }

        if (statement.expression == nullptr) {
            expect_type(current_function_->return_type, make_type(ValueType::unit_type), statement.location);
            return;
        }

        expect_type(current_function_->return_type,
                    check_expression(*statement.expression, expected_type(current_function_->return_type)),
                    statement.expression->location);
        return;
    case StatementKind::expression_statement:
        check_expression(*statement.expression);
        return;
    case StatementKind::import_statement:
        throw std::runtime_error(diagnostic(statement.location, "import statements are only allowed at top level"));
    }
}

void TypeChecker::check_statements(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        check_statement(statement);
    }
}

Type TypeChecker::check_assignment_target(const Expression& target, SourceLocation location) {
    const std::string* root = root_identifier(target);
    if (root != nullptr && has_visible_constant(*root)) {
        throw std::runtime_error(diagnostic(location, "cannot assign through constant '" + *root + "'"));
    }

    Type result = make_type(ValueType::unit_type);
    switch (target.kind) {
    case ExpressionKind::identifier: {
        const VariableBinding* binding = find_binding(target.lexeme);
        if (binding == nullptr) {
            const auto global_constant = global_constants_.find(target.lexeme);
            if (global_constant != global_constants_.end()) {
                throw std::runtime_error(diagnostic(location, "cannot assign to constant '" + target.lexeme + "'"));
            }

            throw std::runtime_error(diagnostic(location, "undefined variable '" + target.lexeme + "'"));
        }

        result = binding->type;
        break;
    }
    case ExpressionKind::index: {
        const Type indexed = check_expression(*target.left);
        if (indexed.kind == ValueType::text_type) {
            throw std::runtime_error(diagnostic(target.left->location, "cannot assign to text index"));
        }

        if (indexed.kind != ValueType::array_type || indexed.element == nullptr) {
            throw std::runtime_error(
                diagnostic(target.left->location, "expected array type but got '" + type_name(indexed) + "'"));
        }

        const Type index = check_expression(*target.right);
        if (!is_integer_type(index.kind)) {
            throw std::runtime_error(
                diagnostic(target.right->location, "expected integer index but got '" + type_name(index) + "'"));
        }

        result = *indexed.element;
        break;
    }
    case ExpressionKind::member: {
        if (target.left != nullptr && target.left->kind == ExpressionKind::identifier &&
            is_known_module(target.left->lexeme)) {
            throw std::runtime_error(diagnostic(location, "cannot assign to module member '" + target.left->lexeme +
                                                              "." + target.lexeme + "'"));
        }

        result = check_member_expression(target, {});
        break;
    }
    case ExpressionKind::number:
    case ExpressionKind::floating:
    case ExpressionKind::character:
    case ExpressionKind::string:
    case ExpressionKind::boolean:
    case ExpressionKind::array:
    case ExpressionKind::struct_literal:
    case ExpressionKind::slice:
    case ExpressionKind::unary:
    case ExpressionKind::cast:
    case ExpressionKind::binary:
    case ExpressionKind::when_expression:
    case ExpressionKind::call:
    case ExpressionKind::method_call:
        throw std::runtime_error(diagnostic(location, "invalid assignment target"));
    }

    expression_types_[&target] = result;
    return result;
}

Type TypeChecker::check_expression(const Expression& expression, const TypeAnnotation& expected) {
    TypeAnnotation wanted = expected;
    if (wanted.has_type) {
        wanted.type = normalize_type(wanted.type);
    }

    Type actual = make_type(ValueType::int_type);

    switch (expression.kind) {
    case ExpressionKind::identifier: {
        const VariableBinding* variable = find_binding(expression.lexeme);
        if (variable != nullptr) {
            actual = variable->type;
            break;
        }

        const auto global_constant = global_constants_.find(expression.lexeme);
        if (global_constant != global_constants_.end()) {
            actual = global_constant->second;
            break;
        }

        if (wanted.has_type && wanted.type.kind == ValueType::enum_type &&
            is_variant_name_for_expected_enum(expression.lexeme, wanted)) {
            actual = check_variant_constructor(expression, expression.lexeme, expression.arguments, expression.location,
                                               wanted);
            break;
        }

        throw std::runtime_error(diagnostic(expression.location, "undefined variable '" + expression.lexeme + "'"));
    }
    case ExpressionKind::number:
        actual = expected.has_type && is_numeric_type(expected.type) ? expected.type : make_type(ValueType::int_type);
        check_integer_literal_range(expression, actual);
        break;
    case ExpressionKind::floating:
        actual =
            expected.has_type && is_real_type(expected.type.kind) ? expected.type : make_type(ValueType::real_type);
        break;
    case ExpressionKind::character:
        actual = make_type(ValueType::glyph_type);
        break;
    case ExpressionKind::string:
        actual = make_type(ValueType::text_type);
        break;
    case ExpressionKind::boolean:
        actual = make_type(ValueType::bool_type);
        break;
    case ExpressionKind::array:
        actual = check_array_literal(expression, wanted);
        break;
    case ExpressionKind::struct_literal:
        actual = check_struct_literal(expression, wanted);
        break;
    case ExpressionKind::index:
        actual = check_index_expression(expression);
        break;
    case ExpressionKind::slice:
        actual = check_slice_expression(expression);
        break;
    case ExpressionKind::member:
        actual = check_member_expression(expression, wanted);
        break;
    case ExpressionKind::unary:
        actual = check_unary_expression(expression);
        break;
    case ExpressionKind::cast:
        actual = check_cast_expression(expression);
        break;
    case ExpressionKind::binary:
        actual = check_binary_expression(expression, wanted);
        break;
    case ExpressionKind::when_expression:
        actual = check_when_expression(expression, wanted);
        break;
    case ExpressionKind::call:
        actual = check_call_expression(expression, wanted);
        break;
    case ExpressionKind::method_call:
        actual = check_method_call_expression(expression, wanted);
        break;
    }

    if (wanted.has_type && !same_type(actual, wanted.type)) {
        actual = coerce_numeric_literal(expression, actual, wanted.type);
    }

    if (wanted.has_type) {
        expect_type(wanted.type, actual, expression.location);
    }

    expression_types_[&expression] = actual;
    return actual;
}

Type TypeChecker::check_binary_expression(const Expression& expression, const TypeAnnotation& expected) {
    if (expression.lexeme == "&&" || expression.lexeme == "||") {
        const Type left = check_expression(*expression.left);
        const Type right = check_expression(*expression.right);
        expect_type(make_type(ValueType::bool_type), left, expression.left->location);
        expect_type(make_type(ValueType::bool_type), right, expression.right->location);
        return make_type(ValueType::bool_type);
    }

    const bool is_arithmetic = expression.lexeme == "+" || expression.lexeme == "-" || expression.lexeme == "*" ||
                               expression.lexeme == "/" || expression.lexeme == "%";
    const TypeAnnotation operand_expected =
        is_arithmetic && expected.has_type && is_numeric_type(expected.type) ? expected : TypeAnnotation{};

    Type left = check_expression(*expression.left, operand_expected);
    Type right = check_expression(*expression.right, operand_expected);

    if (!same_type(left, right)) {
        left = coerce_numeric_literal(*expression.left, left, right);
        right = coerce_numeric_literal(*expression.right, right, left);
    }

    if (is_arithmetic) {
        if (!is_numeric_type(left)) {
            throw std::runtime_error(
                diagnostic(expression.left->location, "expected numeric type but got '" + type_name(left) + "'"));
        }

        if (expression.lexeme == "%" && !is_integer_type(left.kind)) {
            throw std::runtime_error(
                diagnostic(expression.left->location, "expected integer type but got '" + type_name(left) + "'"));
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
        return make_type(ValueType::bool_type);
    }

    if (expression.lexeme == "==" || expression.lexeme == "!=") {
        if (!is_comparable_type(left)) {
            throw std::runtime_error(
                diagnostic(expression.left->location, "cannot compare values of type '" + type_name(left) + "'"));
        }

        expect_type(left, right, expression.right->location);
        return make_type(ValueType::bool_type);
    }

    throw std::runtime_error(diagnostic(expression.location, "unknown binary operator '" + expression.lexeme + "'"));
}

Type TypeChecker::check_when_expression(const Expression& expression, const TypeAnnotation& expected) {
    if (expression.arguments.empty() || expression.arguments.size() % 2 != 0) {
        throw std::runtime_error(diagnostic(expression.location, "when expression needs at least one arm"));
    }

    const Type subject = check_expression(*expression.left);
    if (subject.kind == ValueType::enum_type) {
        return check_enum_when_expression(expression, subject, expected);
    }

    if (!is_comparable_type(subject)) {
        throw std::runtime_error(
            diagnostic(expression.left->location, "cannot apply when to values of type '" + type_name(subject) + "'"));
    }

    bool has_wildcard = false;
    Type result_type;
    bool has_result_type = false;
    for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
        const Expression& pattern = *expression.arguments[index];
        const Expression& result = *expression.arguments[index + 1];
        const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

        if (wildcard) {
            has_wildcard = true;
        } else {
            expect_type(subject, check_expression(pattern, expected_type(subject)), pattern.location);
        }

        const Type value = check_expression(result, has_result_type ? expected_type(result_type) : expected);
        if (!has_result_type) {
            result_type = value;
            has_result_type = true;
        }

        expect_type(result_type, value, result.location);
    }

    if (!has_wildcard) {
        throw std::runtime_error(diagnostic(expression.location, "when expression needs a '_' fallback arm"));
    }

    return result_type;
}

Type TypeChecker::check_call_expression(const Expression& expression, const TypeAnnotation& expected) {
    if (expected.has_type && expected.type.kind == ValueType::enum_type &&
        is_variant_name_for_expected_enum(expression.lexeme, expected)) {
        return check_variant_constructor(expression, expression.lexeme, expression.arguments, expression.location,
                                         expected);
    }

    return check_function_call(expression, expression.lexeme, expression.arguments, expression.location, expected);
}

Type TypeChecker::check_function_call(const Expression& expression, const std::string& name,
                                      const std::vector<std::unique_ptr<Expression>>& arguments,
                                      SourceLocation location, const TypeAnnotation& expected) {
    const std::vector<const Expression*> refs = expression_refs(arguments);
    const FunctionSignature& function = resolve_overload(name, refs, location, expected);
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        expect_type(function.parameters[index],
                    check_expression(*arguments[index], expected_type(function.parameters[index])),
                    arguments[index]->location);
    }

    resolved_calls_[&expression] = function.key;
    return function.return_type;
}

Type TypeChecker::check_variant_constructor(const Expression& expression, const std::string& name,
                                            const std::vector<std::unique_ptr<Expression>>& arguments,
                                            SourceLocation location, const TypeAnnotation& expected) {
    const EnumDefinition* definition = find_expected_enum(expected);
    if (definition == nullptr) {
        throw std::runtime_error(diagnostic(location, "choice variant '" + name + "' needs an expected choice type"));
    }

    const EnumVariant* variant = find_enum_variant(*definition, name);
    if (variant == nullptr) {
        throw std::runtime_error(
            diagnostic(location, "choice '" + type_name(expected.type) + "' has no variant '" + base_name(name) + "'"));
    }

    const std::size_t expected_arguments = variant->has_payload ? 1 : 0;
    if (arguments.size() != expected_arguments) {
        if (!variant->has_payload && !arguments.empty()) {
            throw std::runtime_error(
                diagnostic(location, "choice variant '" + base_name(name) + "' does not have a payload"));
        }

        throw std::runtime_error(diagnostic(location, "choice variant '" + base_name(name) + "' expects " +
                                                          std::to_string(expected_arguments) + " arguments but got " +
                                                          std::to_string(arguments.size())));
    }

    Type payload_type = make_type(ValueType::unit_type);
    if (variant->has_payload) {
        payload_type = substitute_enum_payload(*definition, expected.type, *variant);
        expect_type(payload_type, check_expression(*arguments[0], expected_type(payload_type)), arguments[0]->location);
    }

    resolved_variants_[&expression] =
        VariantResolution{definition->name, variant->name, variant->tag, variant->has_payload, payload_type, "", false};
    return expected.type;
}

Type TypeChecker::check_method_call_expression(const Expression& expression, const TypeAnnotation& expected) {
    if (expression.left != nullptr && expression.left->kind == ExpressionKind::identifier) {
        const std::string module_name = expression.left->lexeme;
        if (is_known_module(module_name)) {
            expect_imported_module(module_name, expression.location);
            expect_exported_member(module_name, expression.lexeme, expression.location);
            const std::string qualified_name = module_name + "." + expression.lexeme;
            if (expected.has_type && expected.type.kind == ValueType::enum_type &&
                is_variant_name_for_expected_enum(qualified_name, expected)) {
                return check_variant_constructor(expression, qualified_name, expression.arguments, expression.location,
                                                 expected);
            }

            return check_function_call(expression, qualified_name, expression.arguments, expression.location, expected);
        }

        if (expected.has_type && expected.type.kind == ValueType::enum_type &&
            (expected.type.name == module_name || base_name(expected.type.name) == module_name)) {
            const std::string qualified_name = module_name + "." + expression.lexeme;
            if (is_variant_name_for_expected_enum(qualified_name, expected)) {
                return check_variant_constructor(expression, qualified_name, expression.arguments, expression.location,
                                                 expected);
            }
        }
    }

    const Type receiver = check_expression(*expression.left);
    if (receiver.kind == ValueType::array_type &&
        (expression.lexeme == "len" || expression.lexeme == "push" || expression.lexeme == "pop" ||
         expression.lexeme == "clear" || expression.lexeme == "is_empty")) {
        return check_array_method_call(receiver, expression);
    }

    if (receiver.kind == ValueType::text_type &&
        (expression.lexeme == "len" || expression.lexeme == "is_empty" || expression.lexeme == "contains" ||
         expression.lexeme == "starts_with")) {
        return check_text_method_call(receiver, expression);
    }

    return check_receiver_method_call(expression, expected);
}

Type TypeChecker::check_receiver_method_call(const Expression& expression, const TypeAnnotation& expected) {
    std::vector<const Expression*> arguments;
    arguments.reserve(expression.arguments.size() + 1);
    arguments.push_back(expression.left.get());
    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        arguments.push_back(argument.get());
    }

    const FunctionSignature* best_match =
        try_resolve_overload(expression.lexeme, arguments, expression.location, expected);
    for (const std::string& module : imports_) {
        const auto exports = module_exports_.find(module);
        if (exports == module_exports_.end() || !exports->second.contains(expression.lexeme)) {
            continue;
        }

        const FunctionSignature* match =
            try_resolve_overload(module + "." + expression.lexeme, arguments, expression.location, expected);
        if (match == nullptr) {
            continue;
        }

        if (best_match != nullptr && best_match->key != match->key) {
            throw std::runtime_error(diagnostic(expression.location, "ambiguous method '" + expression.lexeme + "'"));
        }

        best_match = match;
    }

    if (best_match != nullptr) {
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            expect_type(best_match->parameters[index],
                        check_expression(*arguments[index], expected_type(best_match->parameters[index])),
                        arguments[index]->location);
        }

        resolved_calls_[&expression] = best_match->key;
        return best_match->return_type;
    }

    const Type receiver = expression_types_.contains(expression.left.get())
                              ? expression_types_.at(expression.left.get())
                              : check_expression(*expression.left);
    throw std::runtime_error(diagnostic(expression.location, "type '" + type_name(receiver) + "' has no method '" +
                                                                 expression.lexeme + "'"));
}

Type TypeChecker::check_unary_expression(const Expression& expression) {
    Type right = check_expression(*expression.right);
    if (expression.lexeme == "-") {
        if (!is_numeric_type(right)) {
            throw std::runtime_error(
                diagnostic(expression.right->location, "expected numeric type but got '" + type_name(right) + "'"));
        }

        return right;
    }

    if (expression.lexeme == "!") {
        expect_type(make_type(ValueType::bool_type), right, expression.right->location);
        return make_type(ValueType::bool_type);
    }

    throw std::runtime_error(diagnostic(expression.location, "unknown unary operator '" + expression.lexeme + "'"));
}

Type TypeChecker::check_cast_expression(const Expression& expression) {
    if (!expression.type.has_type) {
        throw std::runtime_error(diagnostic(expression.location, "expected cast target type"));
    }

    const Type source = check_expression(*expression.left);
    const Type target = normalize_type(expression.type.type);
    if (!is_cast_allowed(source, target)) {
        throw std::runtime_error(diagnostic(expression.location, "cannot cast from '" + type_name(source) + "' to '" +
                                                                     type_name(target) + "'"));
    }

    return target;
}

Type TypeChecker::check_member_expression(const Expression& expression, const TypeAnnotation& expected) {
    if (expression.left != nullptr && expression.left->kind == ExpressionKind::identifier) {
        const std::string module_name = expression.left->lexeme;
        if (is_known_module(module_name)) {
            expect_imported_module(module_name, expression.location);
            expect_exported_member(module_name, expression.lexeme, expression.location);
            const std::string qualified_name = module_name + "." + expression.lexeme;
            if (expected.has_type && expected.type.kind == ValueType::enum_type &&
                is_variant_name_for_expected_enum(qualified_name, expected)) {
                return check_variant_constructor(expression, qualified_name, expression.arguments, expression.location,
                                                 expected);
            }

            const VariableBinding* variable = find_binding(qualified_name);
            if (variable != nullptr) {
                return variable->type;
            }

            const auto global_constant = global_constants_.find(qualified_name);
            if (global_constant == global_constants_.end()) {
                throw std::runtime_error(diagnostic(expression.location, "module '" + module_name + "' has no value '" +
                                                                             expression.lexeme + "'"));
            }

            return global_constant->second;
        }

        if (expected.has_type && expected.type.kind == ValueType::enum_type &&
            (expected.type.name == module_name || base_name(expected.type.name) == module_name)) {
            const std::string qualified_name = module_name + "." + expression.lexeme;
            if (is_variant_name_for_expected_enum(qualified_name, expected)) {
                return check_variant_constructor(expression, qualified_name, expression.arguments, expression.location,
                                                 expected);
            }
        }
    }

    const Type receiver = check_expression(*expression.left);
    if (receiver.kind == ValueType::struct_type) {
        const auto definition = structs_.find(receiver.name);
        if (definition == structs_.end()) {
            throw std::runtime_error(diagnostic(expression.location, "unknown record '" + receiver.name + "'"));
        }

        const auto field = definition->second.field_indices.find(expression.lexeme);
        if (field == definition->second.field_indices.end()) {
            throw std::runtime_error(diagnostic(expression.location, "record '" + receiver.name + "' has no field '" +
                                                                         expression.lexeme + "'"));
        }

        if (definition->second.generic_parameters.size() != receiver.arguments.size()) {
            throw std::runtime_error(diagnostic(
                expression.location, "record '" + receiver.name + "' expects " +
                                         std::to_string(definition->second.generic_parameters.size()) +
                                         " type arguments but got " + std::to_string(receiver.arguments.size())));
        }

        std::unordered_map<std::string, Type> substitutions;
        for (std::size_t index = 0; index < definition->second.generic_parameters.size(); ++index) {
            substitutions.emplace(definition->second.generic_parameters[index].name, receiver.arguments[index]);
        }

        return substitute_type(definition->second.fields[field->second].type, substitutions);
    }

    throw std::runtime_error(diagnostic(expression.location, "type '" + type_name(receiver) + "' has no member '" +
                                                                 expression.lexeme + "'"));
}

Type TypeChecker::check_array_method_call(const Type& receiver, const Expression& expression) {
    if (expression.lexeme == "len") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(diagnostic(expression.location, "array method 'len' expects 0 arguments but got " +
                                                                         std::to_string(expression.arguments.size())));
        }

        return make_type(ValueType::int_type);
    }

    if (expression.lexeme == "push") {
        if (expression.arguments.size() != 1) {
            throw std::runtime_error(diagnostic(expression.location, "array method 'push' expects 1 argument but got " +
                                                                         std::to_string(expression.arguments.size())));
        }

        if (receiver.element == nullptr) {
            throw std::runtime_error(
                diagnostic(expression.location, "expected array type but got '" + type_name(receiver) + "'"));
        }

        expect_type(*receiver.element, check_expression(*expression.arguments[0], expected_type(*receiver.element)),
                    expression.arguments[0]->location);
        return make_type(ValueType::unit_type);
    }

    if (expression.lexeme == "pop") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(diagnostic(expression.location, "array method 'pop' expects 0 arguments but got " +
                                                                         std::to_string(expression.arguments.size())));
        }

        if (receiver.element == nullptr) {
            throw std::runtime_error(
                diagnostic(expression.location, "expected array type but got '" + type_name(receiver) + "'"));
        }

        return *receiver.element;
    }

    if (expression.lexeme == "clear") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(
                diagnostic(expression.location, "array method 'clear' expects 0 arguments but got " +
                                                    std::to_string(expression.arguments.size())));
        }

        return make_type(ValueType::unit_type);
    }

    if (expression.lexeme == "is_empty") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(
                diagnostic(expression.location, "array method 'is_empty' expects 0 arguments but got " +
                                                    std::to_string(expression.arguments.size())));
        }

        return make_type(ValueType::bool_type);
    }

    throw std::runtime_error(diagnostic(expression.location, "array type has no method '" + expression.lexeme + "'"));
}

Type TypeChecker::check_text_method_call(const Type& receiver, const Expression& expression) {
    (void)receiver;
    if (expression.lexeme == "len") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(diagnostic(expression.location, "text method 'len' expects 0 arguments but got " +
                                                                         std::to_string(expression.arguments.size())));
        }

        return make_type(ValueType::int_type);
    }

    if (expression.lexeme == "is_empty") {
        if (!expression.arguments.empty()) {
            throw std::runtime_error(
                diagnostic(expression.location, "text method 'is_empty' expects 0 arguments but got " +
                                                    std::to_string(expression.arguments.size())));
        }

        return make_type(ValueType::bool_type);
    }

    if (expression.lexeme == "contains" || expression.lexeme == "starts_with") {
        if (expression.arguments.size() != 1) {
            throw std::runtime_error(diagnostic(expression.location, "text method '" + expression.lexeme +
                                                                         "' expects 1 argument but got " +
                                                                         std::to_string(expression.arguments.size())));
        }

        expect_type(make_type(ValueType::text_type), check_expression(*expression.arguments[0]),
                    expression.arguments[0]->location);
        return make_type(ValueType::bool_type);
    }

    throw std::runtime_error(diagnostic(expression.location, "text type has no method '" + expression.lexeme + "'"));
}

const TypeChecker::FunctionSignature& TypeChecker::resolve_overload(const std::string& name,
                                                                    const std::vector<const Expression*>& arguments,
                                                                    SourceLocation location,
                                                                    const TypeAnnotation& expected) {
    const auto concrete_overload = overloads_.find(name);
    const auto generic_overload = generic_overloads_.find(name);
    if (concrete_overload == overloads_.end() && generic_overload == generic_overloads_.end()) {
        throw std::runtime_error(diagnostic(location, "undefined function '" + name + "'"));
    }

    std::vector<Type> raw_argument_types;
    raw_argument_types.reserve(arguments.size());
    for (const Expression* argument : arguments) {
        raw_argument_types.push_back(check_expression(*argument));
    }

    struct Candidate {
        const FunctionSignature* concrete = nullptr;
        const Statement* generic = nullptr;
        std::unordered_map<std::string, Type> substitutions;
        std::string key;
        int score = -1;
    };

    Candidate best_match;
    int best_score = -1;
    bool ambiguous = false;

    auto consider = [&](Candidate candidate) {
        if (candidate.score > best_score) {
            best_score = candidate.score;
            best_match = std::move(candidate);
            ambiguous = false;
            return;
        }

        if (candidate.score == best_score && candidate.key != best_match.key) {
            ambiguous = true;
        }
    };

    if (concrete_overload != overloads_.end()) {
        for (const std::string& key : concrete_overload->second) {
            const FunctionSignature& function = find_function_by_key(key, location);
            if (function.parameters.size() != arguments.size()) {
                continue;
            }

            bool matches = true;
            int score = 1000;
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                try {
                    const Type actual = check_expression(*arguments[index], expected_type(function.parameters[index]));
                    if (!same_type(actual, function.parameters[index])) {
                        matches = false;
                        break;
                    }
                } catch (const std::runtime_error&) {
                    matches = false;
                    break;
                }

                if (same_type(raw_argument_types[index], function.parameters[index])) {
                    score += 2;
                }
            }

            if (!matches) {
                continue;
            }

            if (expected.has_type && same_type(expected.type, function.return_type)) {
                score += 100;
            }

            consider(Candidate{&function, nullptr, {}, function.key, score});
        }
    }

    if (generic_overload != generic_overloads_.end()) {
        for (const Statement* statement : generic_overload->second) {
            if (statement->parameters.size() != arguments.size()) {
                continue;
            }

            std::unordered_set<std::string> generic_names;
            for (const GenericParameter& parameter : statement->generic_parameters) {
                generic_names.insert(parameter.name);
            }

            std::vector<Type> generic_parameters;
            generic_parameters.reserve(statement->parameters.size());
            for (const Parameter& parameter : statement->parameters) {
                generic_parameters.push_back(annotation_or_default(parameter.type, generic_names));
            }

            std::unordered_map<std::string, std::vector<std::pair<Type, bool>>> constraints;
            if (expected.has_type && !collect_generic_constraints(annotation_or_default(statement->type, generic_names),
                                                                  expected.type, constraints, true)) {
                continue;
            }

            bool constraints_match = true;
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                if (!collect_generic_constraints(generic_parameters[index], raw_argument_types[index], constraints,
                                                 !is_numeric_literal(*arguments[index]))) {
                    constraints_match = false;
                    break;
                }
            }

            if (!constraints_match) {
                continue;
            }

            std::unordered_map<std::string, Type> substitutions;
            bool complete_substitution = true;
            for (const GenericParameter& parameter : statement->generic_parameters) {
                const auto existing_constraints = constraints.find(parameter.name);
                if (existing_constraints == constraints.end() || existing_constraints->second.empty()) {
                    complete_substitution = false;
                    break;
                }

                const Type* chosen = nullptr;
                for (const auto& [type, preferred] : existing_constraints->second) {
                    if (preferred) {
                        chosen = &type;
                        break;
                    }
                }

                if (chosen == nullptr) {
                    chosen = &existing_constraints->second.front().first;
                }

                if (!type_satisfies_bound(*chosen, parameter.bound, parameter.location)) {
                    constraints_match = false;
                    break;
                }

                substitutions.emplace(parameter.name, clone_type(*chosen));
            }

            if (!complete_substitution || !constraints_match) {
                continue;
            }

            std::vector<Type> concrete_parameters;
            concrete_parameters.reserve(generic_parameters.size());
            for (const Type& parameter : generic_parameters) {
                concrete_parameters.push_back(substitute_type(parameter, substitutions));
            }
            const Type concrete_return =
                substitute_type(annotation_or_default(statement->type, generic_names), substitutions);

            bool matches = true;
            int score = 0;
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                try {
                    const Type actual = check_expression(*arguments[index], expected_type(concrete_parameters[index]));
                    if (!same_type(actual, concrete_parameters[index])) {
                        matches = false;
                        break;
                    }
                } catch (const std::runtime_error&) {
                    matches = false;
                    break;
                }

                if (same_type(raw_argument_types[index], concrete_parameters[index])) {
                    score += 2;
                }
            }

            if (!matches) {
                continue;
            }

            if (expected.has_type && same_type(expected.type, concrete_return)) {
                score += 100;
            }

            consider(Candidate{nullptr, statement, std::move(substitutions),
                               function_key(statement->name, concrete_parameters), score});
        }
    }

    if (best_match.concrete == nullptr && best_match.generic == nullptr) {
        std::string message = "no overload for function '" + name + "' with argument types (";
        for (std::size_t index = 0; index < raw_argument_types.size(); ++index) {
            if (index > 0) {
                message += ", ";
            }

            message += type_name(raw_argument_types[index]);
        }
        message += ")";
        throw std::runtime_error(diagnostic(location, message));
    }

    if (ambiguous) {
        throw std::runtime_error(diagnostic(location, "ambiguous overload for function '" + name + "'"));
    }

    if (best_match.concrete != nullptr) {
        return *best_match.concrete;
    }

    return instantiate_generic_function(*best_match.generic, best_match.substitutions, location);
}

const TypeChecker::FunctionSignature* TypeChecker::try_resolve_overload(const std::string& name,
                                                                        const std::vector<const Expression*>& arguments,
                                                                        SourceLocation location,
                                                                        const TypeAnnotation& expected) {
    try {
        return &resolve_overload(name, arguments, location, expected);
    } catch (const std::runtime_error&) {
        return nullptr;
    }
}

const TypeChecker::FunctionSignature& TypeChecker::instantiate_generic_function(
    const Statement& statement, const std::unordered_map<std::string, Type>& substitutions, SourceLocation location) {
    Statement concrete = clone_statement(statement);
    concrete.generic_parameters.clear();
    substitute_statement(concrete, substitutions);

    std::vector<Type> parameters;
    parameters.reserve(concrete.parameters.size());
    for (const Parameter& parameter : concrete.parameters) {
        parameters.push_back(annotation_or_default(parameter.type));
    }

    const std::string key = function_key(concrete.name, parameters);
    if (functions_.contains(key)) {
        return find_function_by_key(key, location);
    }

    instantiated_function_traces_.emplace(key, instantiation_trace(statement, substitutions));
    instantiated_functions_.push_back(std::move(concrete));
    instantiated_function_keys_.insert(key);
    collect_function(instantiated_functions_.back());
    return find_function_by_key(key, location);
}

std::string TypeChecker::instantiation_trace(const Statement& statement,
                                             const std::unordered_map<std::string, Type>& substitutions) const {
    std::string trace = statement.name + "<";
    for (std::size_t index = 0; index < statement.generic_parameters.size(); ++index) {
        if (index > 0) {
            trace += ", ";
        }

        const GenericParameter& parameter = statement.generic_parameters[index];
        trace += parameter.name + " = ";
        const auto replacement = substitutions.find(parameter.name);
        trace += replacement == substitutions.end() ? "unknown" : type_name(replacement->second);
    }
    trace += ">";
    return trace;
}

const TypeChecker::FunctionSignature& TypeChecker::find_function_by_key(const std::string& key,
                                                                        SourceLocation location) const {
    const auto function = functions_.find(key);
    if (function == functions_.end()) {
        throw std::runtime_error(diagnostic(location, "undefined function overload '" + key + "'"));
    }

    return function->second;
}

bool TypeChecker::is_numeric_literal(const Expression& expression) const {
    return expression.kind == ExpressionKind::number || expression.kind == ExpressionKind::floating;
}

bool TypeChecker::type_satisfies_bound(const Type& type, const std::string& bound, SourceLocation location) const {
    if (bound.empty()) {
        return type.kind != ValueType::unit_type;
    }

    if (bound == "integer") {
        return is_integer_type(type.kind);
    }

    if (bound == "numeric") {
        return is_numeric_type(type);
    }

    if (bound == "real") {
        return is_real_type(type.kind);
    }

    if (bound == "comparable") {
        return is_comparable_type(type);
    }

    if (bound == "ordered") {
        return is_ordered_type(type);
    }

    throw std::runtime_error(diagnostic(location, "unknown generic bound '" + bound + "'"));
}

const TypeChecker::EnumVariant* TypeChecker::find_enum_variant(const EnumDefinition& definition,
                                                               const std::string& name) const {
    const auto exact = definition.variant_indices.find(name);
    if (exact != definition.variant_indices.end()) {
        return &definition.variants[exact->second];
    }

    const std::string wanted = base_name(name);
    for (const EnumVariant& variant : definition.variants) {
        if (base_name(variant.name) == wanted) {
            return &variant;
        }
    }

    return nullptr;
}

const TypeChecker::EnumDefinition* TypeChecker::find_expected_enum(const TypeAnnotation& expected) const {
    if (!expected.has_type || expected.type.kind != ValueType::enum_type) {
        return nullptr;
    }

    const auto definition = enums_.find(expected.type.name);
    return definition == enums_.end() ? nullptr : &definition->second;
}

Type TypeChecker::substitute_enum_payload(const EnumDefinition& definition, const Type& enum_type,
                                          const EnumVariant& variant) const {
    if (!variant.has_payload) {
        return make_type(ValueType::unit_type);
    }

    if (definition.generic_parameters.size() != enum_type.arguments.size()) {
        throw std::runtime_error(diagnostic(variant.location, "choice '" + definition.name + "' expects " +
                                                                  std::to_string(definition.generic_parameters.size()) +
                                                                  " type arguments but got " +
                                                                  std::to_string(enum_type.arguments.size())));
    }

    std::unordered_map<std::string, Type> substitutions;
    for (std::size_t index = 0; index < definition.generic_parameters.size(); ++index) {
        substitutions.emplace(definition.generic_parameters[index].name, enum_type.arguments[index]);
    }

    return substitute_type(variant.payload_type, substitutions);
}

bool TypeChecker::is_variant_name_for_expected_enum(const std::string& name, const TypeAnnotation& expected) const {
    const EnumDefinition* definition = find_expected_enum(expected);
    return definition != nullptr && find_enum_variant(*definition, name) != nullptr;
}

TypeChecker::VariantResolution TypeChecker::resolve_variant_pattern(const Expression& pattern, const Type& subject) {
    const auto definition = enums_.find(subject.name);
    if (definition == enums_.end()) {
        throw std::runtime_error(diagnostic(pattern.location, "unknown choice '" + subject.name + "'"));
    }

    std::string variant_name;
    const std::vector<std::unique_ptr<Expression>>* arguments = nullptr;
    if (pattern.kind == ExpressionKind::identifier || pattern.kind == ExpressionKind::call) {
        variant_name = pattern.lexeme;
        arguments = &pattern.arguments;
    } else if ((pattern.kind == ExpressionKind::member || pattern.kind == ExpressionKind::method_call) &&
               pattern.left != nullptr && pattern.left->kind == ExpressionKind::identifier) {
        variant_name = pattern.left->lexeme + "." + pattern.lexeme;
        arguments = &pattern.arguments;
    } else {
        throw std::runtime_error(diagnostic(pattern.location, "expected choice variant pattern"));
    }

    const EnumVariant* variant = find_enum_variant(definition->second, variant_name);
    if (variant == nullptr) {
        throw std::runtime_error(diagnostic(pattern.location, "choice '" + type_name(subject) + "' has no variant '" +
                                                                  base_name(variant_name) + "'"));
    }

    const std::size_t argument_count = arguments == nullptr ? 0 : arguments->size();
    const std::size_t expected_arguments = variant->has_payload ? 1 : 0;
    if (argument_count != expected_arguments) {
        if (!variant->has_payload && argument_count > 0) {
            throw std::runtime_error(diagnostic(pattern.location, "choice variant '" + base_name(variant_name) +
                                                                      "' does not have a payload"));
        }

        throw std::runtime_error(diagnostic(pattern.location, "choice variant '" + base_name(variant_name) +
                                                                  "' expects " + std::to_string(expected_arguments) +
                                                                  " pattern arguments but got " +
                                                                  std::to_string(argument_count)));
    }

    Type payload_type = substitute_enum_payload(definition->second, subject, *variant);
    VariantResolution resolution{
        definition->second.name, variant->name, variant->tag, variant->has_payload, payload_type, "", false};
    if (variant->has_payload) {
        const Expression& binding = *arguments->front();
        if (binding.kind != ExpressionKind::identifier) {
            throw std::runtime_error(diagnostic(binding.location, "expected binding name in choice variant pattern"));
        }

        if (binding.lexeme != "_") {
            resolution.binding_name = binding.lexeme;
            resolution.binds_payload = true;
        }
    }

    resolved_variants_[&pattern] = resolution;
    return resolution;
}

Type TypeChecker::check_enum_when_expression(const Expression& expression, const Type& subject,
                                             const TypeAnnotation& expected) {
    const auto definition = enums_.find(subject.name);
    if (definition == enums_.end()) {
        throw std::runtime_error(diagnostic(expression.left->location, "unknown choice '" + subject.name + "'"));
    }

    std::unordered_set<std::size_t> covered_tags;
    bool has_wildcard = false;
    Type result_type;
    bool has_result_type = false;

    for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
        const Expression& pattern = *expression.arguments[index];
        const Expression& result = *expression.arguments[index + 1];
        const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";

        std::string binding_name;
        Type binding_type;
        bool has_binding = false;
        if (wildcard) {
            has_wildcard = true;
        } else {
            VariantResolution resolution = resolve_variant_pattern(pattern, subject);
            covered_tags.insert(resolution.tag);
            if (resolution.binds_payload) {
                binding_name = resolution.binding_name;
                binding_type = resolution.payload_type;
                has_binding = true;
            }
        }

        push_scope();
        if (has_binding) {
            declare_binding(binding_name, binding_type, false, pattern.location);
        }

        const Type value = check_expression(result, has_result_type ? expected_type(result_type) : expected);
        pop_scope();

        if (!has_result_type) {
            result_type = value;
            has_result_type = true;
        }

        expect_type(result_type, value, result.location);
    }

    if (!has_wildcard && covered_tags.size() != definition->second.variants.size()) {
        throw std::runtime_error(diagnostic(expression.location, "when expression does not cover every variant of '" +
                                                                     type_name(subject) + "'"));
    }

    return result_type;
}

bool TypeChecker::collect_generic_constraints(
    const Type& pattern, const Type& actual,
    std::unordered_map<std::string, std::vector<std::pair<Type, bool>>>& constraints, bool preferred) const {
    if (pattern.kind == ValueType::generic_type) {
        constraints[pattern.name].push_back(std::pair<Type, bool>{clone_type(actual), preferred});
        return true;
    }

    if (pattern.kind == ValueType::array_type) {
        if (actual.kind != ValueType::array_type || pattern.element == nullptr || actual.element == nullptr) {
            return false;
        }

        return collect_generic_constraints(*pattern.element, *actual.element, constraints, preferred);
    }

    if (pattern.kind == ValueType::struct_type || pattern.kind == ValueType::enum_type) {
        if (actual.kind != pattern.kind || pattern.name != actual.name ||
            pattern.arguments.size() != actual.arguments.size()) {
            return false;
        }

        for (std::size_t index = 0; index < pattern.arguments.size(); ++index) {
            if (!collect_generic_constraints(pattern.arguments[index], actual.arguments[index], constraints,
                                             preferred)) {
                return false;
            }
        }

        return true;
    }

    return same_type(pattern, actual);
}

Type TypeChecker::check_array_literal(const Expression& expression, const TypeAnnotation& expected) {
    if (expected.has_type && expected.type.kind != ValueType::array_type) {
        expect_type(expected.type, make_array_type(make_type(ValueType::unit_type)), expression.location);
    }

    if (expression.arguments.empty()) {
        if (!expected.has_type || expected.type.kind != ValueType::array_type) {
            throw std::runtime_error(diagnostic(expression.location, "empty array literal needs an array type"));
        }

        return expected.type;
    }

    Type element_type;
    std::size_t start = 0;
    if (expected.has_type && expected.type.kind == ValueType::array_type && expected.type.element != nullptr) {
        element_type = *expected.type.element;
    } else {
        element_type = check_expression(*expression.arguments[0]);
        start = 1;
    }

    for (std::size_t index = start; index < expression.arguments.size(); ++index) {
        expect_type(element_type, check_expression(*expression.arguments[index], expected_type(element_type)),
                    expression.arguments[index]->location);
    }

    return make_array_type(element_type);
}

Type TypeChecker::check_struct_literal(const Expression& expression, const TypeAnnotation& expected) {
    const auto definition = structs_.find(expression.lexeme);
    if (definition == structs_.end()) {
        throw std::runtime_error(diagnostic(expression.location, "unknown record '" + expression.lexeme + "'"));
    }

    Type result_type = make_struct_type(expression.lexeme);
    if (expected.has_type && expected.type.kind == ValueType::struct_type && expected.type.name == expression.lexeme) {
        result_type = expected.type;
        expect_type(expected.type, result_type, expression.location);
    } else if (!definition->second.generic_parameters.empty()) {
        throw std::runtime_error(diagnostic(expression.location, "generic record literal '" + expression.lexeme +
                                                                     "' needs an expected type"));
    } else if (expected.has_type) {
        expect_type(expected.type, result_type, expression.location);
    }

    if (definition->second.generic_parameters.size() != result_type.arguments.size()) {
        throw std::runtime_error(diagnostic(
            expression.location, "record '" + expression.lexeme + "' expects " +
                                     std::to_string(definition->second.generic_parameters.size()) +
                                     " type arguments but got " + std::to_string(result_type.arguments.size())));
    }

    std::unordered_map<std::string, Type> substitutions;
    for (std::size_t index = 0; index < definition->second.generic_parameters.size(); ++index) {
        substitutions.emplace(definition->second.generic_parameters[index].name, result_type.arguments[index]);
    }

    if (expression.field_names.size() != expression.arguments.size()) {
        throw std::runtime_error(diagnostic(expression.location, "invalid record literal"));
    }

    std::unordered_set<std::string> seen_fields;
    for (std::size_t index = 0; index < expression.field_names.size(); ++index) {
        const std::string& field_name = expression.field_names[index];
        if (!seen_fields.insert(field_name).second) {
            throw std::runtime_error(diagnostic(expression.arguments[index]->location,
                                                "duplicate field '" + field_name + "' in record literal"));
        }

        const auto field = definition->second.field_indices.find(field_name);
        if (field == definition->second.field_indices.end()) {
            throw std::runtime_error(
                diagnostic(expression.arguments[index]->location,
                           "record '" + expression.lexeme + "' has no field '" + field_name + "'"));
        }

        const Type field_type = substitute_type(definition->second.fields[field->second].type, substitutions);
        expect_type(field_type, check_expression(*expression.arguments[index], expected_type(field_type)),
                    expression.arguments[index]->location);
    }

    for (const StructField& field : definition->second.fields) {
        if (!seen_fields.contains(field.name)) {
            throw std::runtime_error(diagnostic(expression.location, "missing field '" + field.name + "' for record '" +
                                                                         expression.lexeme + "'"));
        }
    }

    return result_type;
}

Type TypeChecker::check_index_expression(const Expression& expression) {
    const Type indexed = check_expression(*expression.left);
    if (indexed.kind != ValueType::array_type && indexed.kind != ValueType::text_type) {
        throw std::runtime_error(
            diagnostic(expression.left->location, "expected array or text type but got '" + type_name(indexed) + "'"));
    }

    const Type index = check_expression(*expression.right);
    if (!is_integer_type(index.kind)) {
        throw std::runtime_error(
            diagnostic(expression.right->location, "expected integer index but got '" + type_name(index) + "'"));
    }

    if (indexed.kind == ValueType::text_type) {
        return make_type(ValueType::glyph_type);
    }

    if (indexed.element == nullptr) {
        throw std::runtime_error(
            diagnostic(expression.left->location, "expected array type but got '" + type_name(indexed) + "'"));
    }

    return *indexed.element;
}

Type TypeChecker::check_slice_expression(const Expression& expression) {
    const Type sliced = check_expression(*expression.left);
    if (sliced.kind != ValueType::array_type && sliced.kind != ValueType::text_type) {
        throw std::runtime_error(
            diagnostic(expression.left->location, "expected array or text type but got '" + type_name(sliced) + "'"));
    }

    for (const std::unique_ptr<Expression>& bound : expression.arguments) {
        if (bound == nullptr) {
            continue;
        }

        const Type index = check_expression(*bound);
        if (!is_integer_type(index.kind)) {
            throw std::runtime_error(
                diagnostic(bound->location, "expected integer slice bound but got '" + type_name(index) + "'"));
        }
    }

    if (sliced.kind == ValueType::array_type && sliced.element == nullptr) {
        throw std::runtime_error(
            diagnostic(expression.left->location, "expected array type but got '" + type_name(sliced) + "'"));
    }

    return sliced;
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
    case StatementKind::binding:
    case StatementKind::const_statement:
    case StatementKind::assign:
    case StatementKind::print:
    case StatementKind::while_statement:
    case StatementKind::for_statement:
    case StatementKind::function:
    case StatementKind::method_block:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
    case StatementKind::break_statement:
    case StatementKind::continue_statement:
    case StatementKind::expression_statement:
    case StatementKind::import_statement:
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

Type TypeChecker::annotation_or_default(const TypeAnnotation& annotation) const {
    return annotation_or_default(annotation, {});
}

Type TypeChecker::annotation_or_default(const TypeAnnotation& annotation,
                                        const std::unordered_set<std::string>& generic_parameters) const {
    if (annotation.has_type) {
        return normalize_type(annotation.type, generic_parameters);
    }

    return make_type(ValueType::int_type);
}

Type TypeChecker::normalize_type(const Type& type, const std::unordered_set<std::string>& generic_parameters) const {
    if (type.kind == ValueType::array_type) {
        Type result{ValueType::array_type, nullptr};
        if (type.element != nullptr) {
            result.element = std::make_shared<Type>(normalize_type(*type.element, generic_parameters));
        }
        return result;
    }

    std::vector<Type> arguments;
    arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        arguments.push_back(normalize_type(argument, generic_parameters));
    }

    if (type.kind == ValueType::generic_type && !generic_parameters.contains(type.name) &&
        structs_.contains(type.name)) {
        return make_struct_type(type.name, std::move(arguments));
    }

    if (type.kind == ValueType::generic_type && !generic_parameters.contains(type.name) && enums_.contains(type.name)) {
        return make_enum_type(type.name, std::move(arguments));
    }

    Type result = type;
    result.arguments = std::move(arguments);
    return result;
}

bool TypeChecker::same_type(const Type& left, const Type& right) const {
    if (left.kind != right.kind) {
        return false;
    }

    if (left.kind == ValueType::generic_type || left.kind == ValueType::struct_type ||
        left.kind == ValueType::enum_type) {
        if (left.name != right.name || left.arguments.size() != right.arguments.size()) {
            return false;
        }

        for (std::size_t index = 0; index < left.arguments.size(); ++index) {
            if (!same_type(left.arguments[index], right.arguments[index])) {
                return false;
            }
        }

        return true;
    }

    if (left.kind != ValueType::array_type) {
        return true;
    }

    if (left.element == nullptr || right.element == nullptr) {
        return left.element == right.element;
    }

    return same_type(*left.element, *right.element);
}

bool TypeChecker::is_integer_type(ValueType type) const {
    return is_signed_type(type) || is_unsigned_type(type);
}

bool TypeChecker::is_signed_type(ValueType type) const {
    return type == ValueType::int_type || type == ValueType::i8_type || type == ValueType::i16_type ||
           type == ValueType::i32_type || type == ValueType::i64_type || type == ValueType::isize_type;
}

bool TypeChecker::is_unsigned_type(ValueType type) const {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type || type == ValueType::usize_type;
}

bool TypeChecker::is_real_type(ValueType type) const {
    return type == ValueType::real32_type || type == ValueType::real_type;
}

bool TypeChecker::is_numeric_type(const Type& type) const {
    return is_integer_type(type.kind) || is_real_type(type.kind);
}

bool TypeChecker::is_comparable_type(const Type& type) const {
    return is_numeric_type(type) || type.kind == ValueType::bool_type || type.kind == ValueType::glyph_type ||
           type.kind == ValueType::text_type;
}

bool TypeChecker::is_ordered_type(const Type& type) const {
    return is_numeric_type(type);
}

bool TypeChecker::is_cast_allowed(const Type& source, const Type& target) const {
    if (same_type(source, target)) {
        return true;
    }

    if (source.kind == ValueType::array_type || target.kind == ValueType::array_type ||
        source.kind == ValueType::struct_type || target.kind == ValueType::struct_type ||
        source.kind == ValueType::enum_type || target.kind == ValueType::enum_type ||
        source.kind == ValueType::unit_type || target.kind == ValueType::unit_type ||
        source.kind == ValueType::text_type || target.kind == ValueType::text_type) {
        return false;
    }

    const bool source_scalar =
        is_numeric_type(source) || source.kind == ValueType::bool_type || source.kind == ValueType::glyph_type;
    const bool target_scalar =
        is_numeric_type(target) || target.kind == ValueType::bool_type || target.kind == ValueType::glyph_type;
    return source_scalar && target_scalar;
}

bool TypeChecker::can_coerce_integer_literal(const Expression& expression, const Type& target) const {
    if (expression.kind == ExpressionKind::number && is_numeric_type(target)) {
        return true;
    }

    return expression.kind == ExpressionKind::floating && is_real_type(target.kind);
}

void TypeChecker::check_integer_literal_range(const Expression& expression, const Type& target) const {
    if (!is_integer_type(target.kind)) {
        return;
    }

    const unsigned long long value = std::stoull(expression.lexeme);
    const unsigned long long max = max_integer_literal(target.kind);
    if (value > max) {
        throw std::runtime_error(
            diagnostic(expression.location,
                       "integer literal '" + expression.lexeme + "' does not fit in type '" + type_name(target) + "'"));
    }
}

unsigned long long TypeChecker::max_integer_literal(ValueType target) const {
    switch (target) {
    case ValueType::i8_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::int8_t>::max());
    case ValueType::i16_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::int16_t>::max());
    case ValueType::i32_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::int32_t>::max());
    case ValueType::int_type:
    case ValueType::i64_type:
    case ValueType::isize_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::int64_t>::max());
    case ValueType::u8_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::uint8_t>::max());
    case ValueType::u16_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::uint16_t>::max());
    case ValueType::u32_type:
        return static_cast<unsigned long long>(std::numeric_limits<std::uint32_t>::max());
    case ValueType::u64_type:
    case ValueType::usize_type:
        return std::numeric_limits<unsigned long long>::max();
    case ValueType::bool_type:
    case ValueType::real32_type:
    case ValueType::real_type:
    case ValueType::glyph_type:
    case ValueType::text_type:
    case ValueType::unit_type:
    case ValueType::array_type:
    case ValueType::generic_type:
    case ValueType::struct_type:
    case ValueType::enum_type:
        break;
    }

    return 0;
}

Type TypeChecker::coerce_numeric_literal(const Expression& expression, const Type& actual, const Type& target) {
    if (!same_type(actual, target) && can_coerce_integer_literal(expression, target)) {
        check_integer_literal_range(expression, target);
        expression_types_[&expression] = target;
        return target;
    }

    return actual;
}

void TypeChecker::reset_scopes() {
    scopes_.clear();
    push_scope();
}

void TypeChecker::push_scope() {
    scopes_.emplace_back();
}

void TypeChecker::pop_scope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

TypeChecker::VariableBinding* TypeChecker::find_binding(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto binding = scope->find(name);
        if (binding != scope->end()) {
            return &binding->second;
        }
    }

    return nullptr;
}

const TypeChecker::VariableBinding* TypeChecker::find_binding(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto binding = scope->find(name);
        if (binding != scope->end()) {
            return &binding->second;
        }
    }

    return nullptr;
}

bool TypeChecker::has_visible_constant(const std::string& name) const {
    const VariableBinding* binding = find_binding(name);
    return (binding != nullptr && binding->constant) || global_constants_.contains(name);
}

void TypeChecker::declare_binding(const std::string& name, const Type& type, bool constant, SourceLocation location) {
    if (scopes_.empty()) {
        push_scope();
    }

    if (scopes_.back().contains(name)) {
        throw std::runtime_error(diagnostic(location, "variable '" + name + "' already declared in this scope"));
    }

    if (has_visible_constant(name)) {
        throw std::runtime_error(diagnostic(location, "cannot shadow constant '" + name + "'"));
    }

    scopes_.back().emplace(name, VariableBinding{type, constant});
}

void TypeChecker::expect_type(const Type& expected, const Type& actual, SourceLocation location) const {
    if (!same_type(expected, actual)) {
        throw std::runtime_error(
            diagnostic(location, "expected type '" + type_name(expected) + "' but got '" + type_name(actual) + "'"));
    }
}

void TypeChecker::expect_imported_module(const std::string& module, SourceLocation location) const {
    if (!imports_.contains(module)) {
        throw std::runtime_error(diagnostic(location, "module '" + module + "' must be imported before use"));
    }
}

void TypeChecker::expect_exported_member(const std::string& module, const std::string& member,
                                         SourceLocation location) const {
    const auto exports = module_exports_.find(module);
    if (exports == module_exports_.end() || !exports->second.contains(member)) {
        throw std::runtime_error(diagnostic(location, "module '" + module + "' does not export '" + member + "'"));
    }
}

void TypeChecker::collect_known_module(const std::string& name) {
    const std::size_t separator = name.find('.');
    if (separator == std::string::npos || separator == 0) {
        return;
    }

    known_modules_.insert(name.substr(0, separator));
}

void TypeChecker::collect_module_export(const Statement& statement) {
    if (!statement.exported ||
        (statement.kind != StatementKind::function && statement.kind != StatementKind::const_statement &&
         statement.kind != StatementKind::struct_statement && statement.kind != StatementKind::enum_statement)) {
        return;
    }

    const std::size_t separator = statement.name.find('.');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= statement.name.size()) {
        return;
    }

    module_exports_[statement.name.substr(0, separator)].insert(statement.name.substr(separator + 1));
    if (statement.kind == StatementKind::enum_statement) {
        const std::string module = statement.name.substr(0, separator);
        for (const Parameter& variant : statement.parameters) {
            module_exports_[module].insert(base_name(variant.name));
        }
    }
}

bool TypeChecker::is_qualified_module_name(const std::string& name) const {
    const std::size_t separator = name.find('.');
    return separator != std::string::npos && separator > 0;
}

bool TypeChecker::is_known_module(const std::string& module) const {
    if (known_modules_.contains(module)) {
        return true;
    }

    const std::string prefix = module + ".";
    for (const auto& [name, signature] : functions_) {
        (void)signature;
        if (name.starts_with(prefix)) {
            return true;
        }
    }

    for (const auto& [name, definition] : structs_) {
        (void)definition;
        if (name.starts_with(prefix)) {
            return true;
        }
    }

    for (const auto& [name, definition] : enums_) {
        (void)definition;
        if (name.starts_with(prefix)) {
            return true;
        }
    }

    return false;
}

std::string TypeChecker::diagnostic(SourceLocation location, const std::string& message) const {
    return "line " + std::to_string(location.line) + ", columns " + std::to_string(location.column) + "-" +
           std::to_string(location.column + location.length - 1) + ": " + message;
}

} // namespace dune
