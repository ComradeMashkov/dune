#include "module_loader.hpp"

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

#ifndef DUNE_STDLIB_PATH
#define DUNE_STDLIB_PATH "stdlib"
#endif

std::vector<std::filesystem::path> default_search_paths() {
    const char* env_path = std::getenv("DUNE_STDLIB_PATH");
    if (env_path == nullptr || *env_path == '\0') {
        return {std::filesystem::path(DUNE_STDLIB_PATH)};
    }

#if defined(_WIN32)
    constexpr char delimiter = ';';
#else
    constexpr char delimiter = ':';
#endif

    std::vector<std::filesystem::path> paths;
    std::stringstream stream(env_path);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        if (!item.empty()) {
            paths.emplace_back(item);
        }
    }

    if (paths.empty()) {
        paths.emplace_back(DUNE_STDLIB_PATH);
    }
    return paths;
}

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
    return "line " + std::to_string(location.line) + ", columns " + std::to_string(location.column) + "-" +
           std::to_string(location.column + location.length - 1) + ": " + message;
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

TypeAnnotation clone_type_annotation(const TypeAnnotation& annotation) {
    if (!annotation.has_type) {
        return {};
    }

    return TypeAnnotation{true, clone_type(annotation.type)};
}

Type make_generic_type(std::string name) {
    Type type{ValueType::generic_type, nullptr};
    type.name = std::move(name);
    return type;
}

TypeAnnotation receiver_type_for_record(const Statement& statement) {
    Type type = make_generic_type(statement.name);
    type.arguments.reserve(statement.generic_parameters.size());
    for (const GenericParameter& parameter : statement.generic_parameters) {
        type.arguments.push_back(make_generic_type(parameter.name));
    }

    return TypeAnnotation{true, std::move(type)};
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
            Parameter{parameter.name, clone_type_annotation(parameter.type), parameter.location, parameter.exported});
    }

    result.generic_parameters = statement.generic_parameters;
    result.contracts.reserve(statement.contracts.size());
    for (const Type& contract : statement.contracts) {
        result.contracts.push_back(clone_type(contract));
    }
    result.location = statement.location;
    result.initializer = clone_statement_pointer(statement.initializer);
    result.increment = clone_statement_pointer(statement.increment);
    result.exported = statement.exported;
    result.is_extern = statement.is_extern;
    result.is_record_member = statement.is_record_member;
    result.is_constructor = statement.is_constructor;
    result.extern_symbol = statement.extern_symbol;
    result.owner_record = statement.owner_record;
    result.target = clone_expression_pointer(statement.target);
    result.arguments.reserve(statement.arguments.size());
    for (const std::unique_ptr<Expression>& argument : statement.arguments) {
        result.arguments.push_back(clone_expression_pointer(argument));
    }
    return result;
}

void desugar_impls(Program& program) {
    std::vector<Statement> desugared;
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::struct_statement && !statement.body.empty()) {
            Statement record = clone_statement(statement);
            desugared.push_back(std::move(record));

            TypeAnnotation receiver_type = receiver_type_for_record(statement);
            for (const Statement& method : statement.body) {
                if (method.kind != StatementKind::function) {
                    throw std::runtime_error(diagnostic(method.location, "record method must be a function"));
                }

                Statement function = clone_statement(method);
                std::vector<GenericParameter> generic_parameters = statement.generic_parameters;
                generic_parameters.insert(generic_parameters.end(), function.generic_parameters.begin(),
                                          function.generic_parameters.end());
                function.generic_parameters = std::move(generic_parameters);
                function.is_record_member = true;
                function.is_constructor = function.name == "new";
                function.owner_record = statement.name;
                if (function.is_constructor) {
                    function.name = statement.name + ".new";
                } else {
                    function.parameters.insert(
                        function.parameters.begin(),
                        Parameter{"this", clone_type_annotation(receiver_type), statement.location});
                }
                desugared.push_back(std::move(function));
            }

            continue;
        }

        if (statement.kind != StatementKind::method_block) {
            desugared.push_back(clone_statement(statement));
            continue;
        }

        if (!statement.type.has_type) {
            throw std::runtime_error(diagnostic(statement.location, "method declaration needs a receiver type"));
        }

        for (const Statement& method : statement.body) {
            if (method.kind != StatementKind::function) {
                throw std::runtime_error(diagnostic(method.location, "method block can only contain functions"));
            }

            Statement function = clone_statement(method);
            std::vector<GenericParameter> generic_parameters = statement.generic_parameters;
            generic_parameters.insert(generic_parameters.end(), function.generic_parameters.begin(),
                                      function.generic_parameters.end());
            function.generic_parameters = std::move(generic_parameters);
            function.parameters.insert(function.parameters.begin(),
                                       Parameter{"this", clone_type_annotation(statement.type), statement.location});
            function.exported = statement.exported || function.exported;
            desugared.push_back(std::move(function));
        }
    }

    program.statements = std::move(desugared);
}

} // namespace

ModuleLoader::ModuleLoader() : ModuleLoader(default_search_paths()) {}

ModuleLoader::ModuleLoader(std::vector<std::filesystem::path> search_paths) : search_paths_(std::move(search_paths)) {}

Program ModuleLoader::resolve(Program program, const std::filesystem::path& source_directory) {
    loaded_modules_.clear();
    desugar_impls(program);

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
    return program;
}

std::vector<Statement> ModuleLoader::load_module(const std::string& module_name,
                                                 const std::filesystem::path& importer_directory) {
    if (!loaded_modules_.insert(module_name).second) {
        return {};
    }

    const std::filesystem::path module_path = find_module(module_name, importer_directory);
    Program module = parse_file(module_path);
    desugar_impls(module);

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
            statement.kind != StatementKind::struct_statement && statement.kind != StatementKind::enum_statement &&
            statement.kind != StatementKind::contract_statement && statement.kind != StatementKind::import_statement) {
            throw std::runtime_error("module '" + module_name +
                                     "' can only contain imports, constants, functions, and "
                                     "types");
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
    std::unordered_set<std::string> local_structs;
    std::unordered_set<std::string> local_contracts;
    bool has_explicit_exports = false;
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            local_functions.insert(statement.name);
        }

        if (statement.kind == StatementKind::struct_statement) {
            local_structs.insert(statement.name);
        }

        if (statement.kind == StatementKind::contract_statement) {
            local_contracts.insert(statement.name);
        }

        if (statement.kind == StatementKind::enum_statement) {
            local_structs.insert(statement.name);
            for (const Parameter& variant : statement.parameters) {
                local_functions.insert(variant.name);
                local_constants.insert(variant.name);
            }
        }

        if (statement.kind == StatementKind::const_statement) {
            local_constants.insert(statement.name);
        }

        if ((statement.kind == StatementKind::function || statement.kind == StatementKind::const_statement ||
             statement.kind == StatementKind::struct_statement || statement.kind == StatementKind::enum_statement ||
             statement.kind == StatementKind::contract_statement) &&
            statement.exported) {
            has_explicit_exports = true;
        }
    }

    for (Statement& statement : program.statements) {
        if (statement.kind == StatementKind::const_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::function) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs, local_contracts);
            if (statement.is_extern && statement.extern_symbol.empty()) {
                statement.extern_symbol = statement.name;
            }
            if (!has_explicit_exports && !statement.is_record_member) {
                statement.exported = true;
            }
            if (!statement.owner_record.empty() && local_structs.contains(statement.owner_record)) {
                statement.owner_record = module_name + "." + statement.owner_record;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::struct_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::enum_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
            for (Parameter& variant : statement.parameters) {
                variant.name = module_name + "." + variant.name;
            }
        }

        if (statement.kind == StatementKind::contract_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }
    }
}

void ModuleLoader::qualify_type_annotation(TypeAnnotation& annotation, const std::string& module_name,
                                           const std::unordered_set<std::string>& local_structs) const {
    if (!annotation.has_type) {
        return;
    }

    qualify_type(annotation.type, module_name, local_structs);
}

void ModuleLoader::qualify_type(Type& type, const std::string& module_name,
                                const std::unordered_set<std::string>& local_structs) const {
    if (type.kind == ValueType::array_type && type.element != nullptr) {
        qualify_type(*type.element, module_name, local_structs);
        return;
    }

    for (Type& argument : type.arguments) {
        qualify_type(argument, module_name, local_structs);
    }

    if (type.kind == ValueType::generic_type && local_structs.contains(type.name)) {
        type.name = module_name + "." + type.name;
    }
}

void ModuleLoader::qualify_generic_parameters(std::vector<GenericParameter>& parameters, const std::string& module_name,
                                              const std::unordered_set<std::string>& local_contracts) const {
    for (GenericParameter& parameter : parameters) {
        if (!parameter.bound.empty() && local_contracts.contains(parameter.bound)) {
            parameter.bound = module_name + "." + parameter.bound;
        }
    }
}

void ModuleLoader::qualify_contracts(std::vector<Type>& contracts, const std::string& module_name,
                                     const std::unordered_set<std::string>& local_contracts) const {
    for (Type& contract : contracts) {
        if (contract.kind == ValueType::generic_type && local_contracts.contains(contract.name)) {
            contract.name = module_name + "." + contract.name;
        }
    }
}

void ModuleLoader::qualify_statement(Statement& statement, const std::string& module_name,
                                     const std::unordered_set<std::string>& local_functions,
                                     const std::unordered_set<std::string>& local_constants,
                                     const std::unordered_set<std::string>& local_structs,
                                     const std::unordered_set<std::string>& local_contracts) const {
    qualify_generic_parameters(statement.generic_parameters, module_name, local_contracts);
    qualify_contracts(statement.contracts, module_name, local_contracts);
    qualify_type_annotation(statement.type, module_name, local_structs);
    for (Parameter& parameter : statement.parameters) {
        qualify_type_annotation(parameter.type, module_name, local_structs);
    }

    if (statement.expression != nullptr) {
        qualify_expression(*statement.expression, module_name, local_functions, local_constants, local_structs);
    }

    for (std::unique_ptr<Expression>& argument : statement.arguments) {
        qualify_expression(*argument, module_name, local_functions, local_constants, local_structs);
    }

    for (Statement& child : statement.body) {
        qualify_statement(child, module_name, local_functions, local_constants, local_structs, local_contracts);
    }

    for (Statement& child : statement.else_body) {
        qualify_statement(child, module_name, local_functions, local_constants, local_structs, local_contracts);
    }

    if (statement.initializer != nullptr) {
        qualify_statement(*statement.initializer, module_name, local_functions, local_constants, local_structs,
                          local_contracts);
    }

    if (statement.increment != nullptr) {
        qualify_statement(*statement.increment, module_name, local_functions, local_constants, local_structs,
                          local_contracts);
    }
}

void ModuleLoader::qualify_expression(Expression& expression, const std::string& module_name,
                                      const std::unordered_set<std::string>& local_functions,
                                      const std::unordered_set<std::string>& local_constants,
                                      const std::unordered_set<std::string>& local_structs) const {
    qualify_type_annotation(expression.type, module_name, local_structs);

    if (expression.kind == ExpressionKind::call && local_functions.contains(expression.lexeme)) {
        expression.lexeme = module_name + "." + expression.lexeme;
    }

    if (expression.kind == ExpressionKind::identifier && local_constants.contains(expression.lexeme)) {
        expression.lexeme = module_name + "." + expression.lexeme;
    }

    if (expression.kind == ExpressionKind::struct_literal && local_structs.contains(expression.lexeme)) {
        expression.lexeme = module_name + "." + expression.lexeme;
    }

    if (expression.kind == ExpressionKind::method_call && expression.left != nullptr &&
        expression.left->kind == ExpressionKind::identifier && local_structs.contains(expression.left->lexeme)) {
        expression.left->lexeme = module_name + "." + expression.left->lexeme;
    }

    if (expression.left != nullptr) {
        qualify_expression(*expression.left, module_name, local_functions, local_constants, local_structs);
    }

    if (expression.right != nullptr) {
        qualify_expression(*expression.right, module_name, local_functions, local_constants, local_structs);
    }

    for (std::unique_ptr<Expression>& argument : expression.arguments) {
        if (argument != nullptr) {
            qualify_expression(*argument, module_name, local_functions, local_constants, local_structs);
        }
    }
}

} // namespace dune
