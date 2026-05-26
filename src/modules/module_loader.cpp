#include "module_loader.hpp"

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace dune {

namespace {

#ifndef DUNE_STDLIB_PATH
#define DUNE_STDLIB_PATH "stdlib"
#endif

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open module file '" + path.string() + "'");
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool is_relative_to_parent(const std::filesystem::path& path) {
    for (const std::filesystem::path& part : path) {
        if (part == "..") {
            return true;
        }
    }

    return false;
}

std::string diagnostic(SourceLocation location, const std::string& message) {
    return "line " + std::to_string(location.line) + ", column " + std::to_string(location.column) + ": " + message;
}

Type clone_type(const Type& type) {
    Type result{type.kind, nullptr};
    result.name = type.name;
    if (type.element != nullptr) {
        result.element = std::make_shared<Type>(clone_type(*type.element));
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

std::vector<Type> unbounded_generic_types() {
    return {
        Type{ValueType::int_type, nullptr},    Type{ValueType::bool_type, nullptr},
        Type{ValueType::i8_type, nullptr},     Type{ValueType::i16_type, nullptr},
        Type{ValueType::i32_type, nullptr},    Type{ValueType::i64_type, nullptr},
        Type{ValueType::isize_type, nullptr},  Type{ValueType::u8_type, nullptr},
        Type{ValueType::u16_type, nullptr},    Type{ValueType::u32_type, nullptr},
        Type{ValueType::u64_type, nullptr},    Type{ValueType::usize_type, nullptr},
        Type{ValueType::real32_type, nullptr}, Type{ValueType::real_type, nullptr},
        Type{ValueType::glyph_type, nullptr},  Type{ValueType::text_type, nullptr},
    };
}

std::vector<Type> integer_generic_types() {
    return {
        Type{ValueType::int_type, nullptr}, Type{ValueType::i8_type, nullptr},    Type{ValueType::i16_type, nullptr},
        Type{ValueType::i32_type, nullptr}, Type{ValueType::i64_type, nullptr},   Type{ValueType::isize_type, nullptr},
        Type{ValueType::u8_type, nullptr},  Type{ValueType::u16_type, nullptr},   Type{ValueType::u32_type, nullptr},
        Type{ValueType::u64_type, nullptr}, Type{ValueType::usize_type, nullptr},
    };
}

std::vector<Type> numeric_generic_types() {
    std::vector<Type> types = integer_generic_types();
    types.push_back(Type{ValueType::real32_type, nullptr});
    types.push_back(Type{ValueType::real_type, nullptr});
    return types;
}

std::vector<Type> real_generic_types() {
    return {
        Type{ValueType::real32_type, nullptr},
        Type{ValueType::real_type, nullptr},
    };
}

std::vector<Type> generic_concrete_types(const GenericParameter& parameter) {
    if (parameter.bound.empty()) {
        return unbounded_generic_types();
    }

    if (parameter.bound == "integer") {
        return integer_generic_types();
    }

    if (parameter.bound == "numeric") {
        return numeric_generic_types();
    }

    if (parameter.bound == "real") {
        return real_generic_types();
    }

    throw std::runtime_error(diagnostic(parameter.location, "unknown generic bound '" + parameter.bound + "'"));
}

void expand_generic_statement(const Statement& statement, std::size_t index,
                              std::unordered_map<std::string, Type>& substitutions, std::vector<Statement>& expanded) {
    if (index == statement.generic_parameters.size()) {
        Statement concrete = clone_statement(statement);
        concrete.generic_parameters.clear();
        substitute_statement(concrete, substitutions);
        expanded.push_back(std::move(concrete));
        return;
    }

    const GenericParameter& parameter = statement.generic_parameters[index];
    for (const Type& type : generic_concrete_types(parameter)) {
        substitutions[parameter.name] = clone_type(type);
        expand_generic_statement(statement, index + 1, substitutions, expanded);
    }
    substitutions.erase(parameter.name);
}

void expand_generics(Program& program) {
    std::vector<Statement> expanded;
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function && !statement.generic_parameters.empty()) {
            std::unordered_map<std::string, Type> substitutions;
            expand_generic_statement(statement, 0, substitutions, expanded);
            continue;
        }

        expanded.push_back(clone_statement(statement));
    }

    program.statements = std::move(expanded);
}

} // namespace

ModuleLoader::ModuleLoader() : ModuleLoader({std::filesystem::path(DUNE_STDLIB_PATH)}) {}

ModuleLoader::ModuleLoader(std::vector<std::filesystem::path> search_paths) : search_paths_(std::move(search_paths)) {}

Program ModuleLoader::resolve(Program program, const std::filesystem::path& source_directory) {
    loaded_modules_.clear();

    std::vector<Statement> resolved_statements;
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::import_statement) {
            std::vector<Statement> module_statements = load_module(statement.name, source_directory);
            resolved_statements.insert(resolved_statements.end(), std::make_move_iterator(module_statements.begin()),
                                       std::make_move_iterator(module_statements.end()));
        }
    }

    resolved_statements.insert(resolved_statements.end(), std::make_move_iterator(program.statements.begin()),
                               std::make_move_iterator(program.statements.end()));
    program.statements = std::move(resolved_statements);
    expand_generics(program);
    return program;
}

std::vector<Statement> ModuleLoader::load_module(const std::string& module_name,
                                                 const std::filesystem::path& importer_directory) {
    if (!loaded_modules_.insert(module_name).second) {
        return {};
    }

    const std::filesystem::path module_path = find_module(module_name, importer_directory);
    Program module = parse_file(module_path);

    std::vector<Statement> statements;
    for (const Statement& statement : module.statements) {
        if (statement.kind == StatementKind::import_statement) {
            std::vector<Statement> imported = load_module(statement.name, module_path.parent_path());
            statements.insert(statements.end(), std::make_move_iterator(imported.begin()),
                              std::make_move_iterator(imported.end()));
        }
    }

    qualify_module_program(module, module_name);
    for (Statement& statement : module.statements) {
        if (statement.kind != StatementKind::function && statement.kind != StatementKind::const_statement &&
            statement.kind != StatementKind::import_statement) {
            throw std::runtime_error("module '" + module_name + "' can only contain imports, constants, and functions");
        }

        statements.push_back(std::move(statement));
    }

    return statements;
}

std::filesystem::path ModuleLoader::find_module(const std::string& module_name,
                                                const std::filesystem::path& importer_directory) const {
    if (module_name.empty() || is_relative_to_parent(std::filesystem::path(module_name))) {
        throw std::runtime_error("invalid module name '" + module_name + "'");
    }

    const std::filesystem::path module_path = std::filesystem::path(module_name).replace_extension(".dn");
    std::vector<std::filesystem::path> candidates;
    if (!importer_directory.empty()) {
        candidates.push_back(importer_directory / module_path);
    }

    for (const std::filesystem::path& search_path : search_paths_) {
        candidates.push_back(search_path / module_path);
    }

    candidates.push_back(std::filesystem::current_path() / module_path);

    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error("unknown module '" + module_name + "'");
}

Program ModuleLoader::parse_file(const std::filesystem::path& path) const {
    Lexer lexer(read_file(path));
    Parser parser(lexer.tokenize());
    return parser.parse();
}

void ModuleLoader::qualify_module_program(Program& program, const std::string& module_name) const {
    std::unordered_set<std::string> local_functions;
    std::unordered_set<std::string> local_constants;
    bool has_explicit_exports = false;
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            local_functions.insert(statement.name);
        }

        if (statement.kind == StatementKind::const_statement) {
            local_constants.insert(statement.name);
        }

        if ((statement.kind == StatementKind::function || statement.kind == StatementKind::const_statement) &&
            statement.exported) {
            has_explicit_exports = true;
        }
    }

    for (Statement& statement : program.statements) {
        if (statement.kind == StatementKind::const_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::function) {
            qualify_statement(statement, module_name, local_functions, local_constants);
            if (statement.is_extern && statement.extern_symbol.empty()) {
                statement.extern_symbol = statement.name;
            }
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }
    }
}

void ModuleLoader::qualify_statement(Statement& statement, const std::string& module_name,
                                     const std::unordered_set<std::string>& local_functions,
                                     const std::unordered_set<std::string>& local_constants) const {
    if (statement.expression != nullptr) {
        qualify_expression(*statement.expression, module_name, local_functions, local_constants);
    }

    for (Statement& child : statement.body) {
        qualify_statement(child, module_name, local_functions, local_constants);
    }

    for (Statement& child : statement.else_body) {
        qualify_statement(child, module_name, local_functions, local_constants);
    }

    if (statement.initializer != nullptr) {
        qualify_statement(*statement.initializer, module_name, local_functions, local_constants);
    }

    if (statement.increment != nullptr) {
        qualify_statement(*statement.increment, module_name, local_functions, local_constants);
    }
}

void ModuleLoader::qualify_expression(Expression& expression, const std::string& module_name,
                                      const std::unordered_set<std::string>& local_functions,
                                      const std::unordered_set<std::string>& local_constants) const {
    if (expression.kind == ExpressionKind::call && local_functions.contains(expression.lexeme)) {
        expression.lexeme = module_name + "." + expression.lexeme;
    }

    if (expression.kind == ExpressionKind::identifier && local_constants.contains(expression.lexeme)) {
        expression.lexeme = module_name + "." + expression.lexeme;
    }

    if (expression.left != nullptr) {
        qualify_expression(*expression.left, module_name, local_functions, local_constants);
    }

    if (expression.right != nullptr) {
        qualify_expression(*expression.right, module_name, local_functions, local_constants);
    }

    for (std::unique_ptr<Expression>& argument : expression.arguments) {
        if (argument != nullptr) {
            qualify_expression(*argument, module_name, local_functions, local_constants);
        }
    }
}

} // namespace dune
