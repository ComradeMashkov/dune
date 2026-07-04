#include "module_loader.hpp"

#include "diagnostics/diagnostic.hpp"
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

std::shared_ptr<Expression> clone_expression_pointer(const std::shared_ptr<Expression>& expression) {
    if (expression == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Expression> cloned = clone_expression(*expression);
    return std::shared_ptr<Expression>(std::move(cloned));
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
        result.parameters.push_back(Parameter{parameter.name, clone_type_annotation(parameter.type), parameter.location,
                                              parameter.exported, clone_expression_pointer(parameter.default_value),
                                              parameter.doc_comment});
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
    result.is_static_record_member = statement.is_static_record_member;
    result.extern_symbol = statement.extern_symbol;
    result.owner_record = statement.owner_record;
    result.target = clone_expression_pointer(statement.target);
    result.arguments.reserve(statement.arguments.size());
    for (const std::unique_ptr<Expression>& argument : statement.arguments) {
        result.arguments.push_back(clone_expression_pointer(argument));
    }
    result.module_alias = statement.module_alias;
    result.import_symbols = statement.import_symbols;
    result.doc_comment = statement.doc_comment;
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
                    throw DiagnosticError(method.location, "record method must be a function");
                }

                Statement function = clone_statement(method);
                std::vector<GenericParameter> generic_parameters = statement.generic_parameters;
                generic_parameters.insert(generic_parameters.end(), function.generic_parameters.begin(),
                                          function.generic_parameters.end());
                function.generic_parameters = std::move(generic_parameters);
                function.is_record_member = true;
                function.is_constructor = function.name == "new";
                function.is_static_record_member = function.is_static_record_member || function.is_constructor;
                function.owner_record = statement.name;
                if (function.is_constructor || function.is_static_record_member) {
                    function.name = statement.name + "." + function.name;
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
            throw DiagnosticError(statement.location, "method declaration needs a receiver type");
        }

        for (const Statement& method : statement.body) {
            if (method.kind != StatementKind::function) {
                throw DiagnosticError(method.location, "method block can only contain functions");
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
    module_exports_.clear();
    desugar_impls(program);

    // Load every imported module (recording aliases / selective imports), then
    // rewrite the top-level file's own references to the canonical `module.symbol`
    // names before prepending the resolved module statements.
    ImportContext context;
    std::vector<Statement> resolved_statements = collect_imports(program.statements, source_directory, context);

    std::vector<Statement> own = rewrite_file(program.statements, context);
    resolved_statements.insert(resolved_statements.end(), std::make_move_iterator(own.begin()),
                               std::make_move_iterator(own.end()));
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

    // Resolve the module's own imports (including its aliases / selective imports),
    // rewrite its references accordingly, then qualify its local declarations with
    // the module name.
    ImportContext context;
    std::vector<Statement> statements = collect_imports(module.statements, module_path.parent_path(), context);

    Program own;
    own.statements = rewrite_file(module.statements, context);
    qualify_module_program(own, module_name);
    record_module_exports(module_name, own.statements);

    for (Statement& statement : own.statements) {
        if (statement.kind != StatementKind::function && statement.kind != StatementKind::const_statement &&
            statement.kind != StatementKind::struct_statement && statement.kind != StatementKind::enum_statement &&
            statement.kind != StatementKind::contract_statement &&
            statement.kind != StatementKind::type_alias_statement &&
            statement.kind != StatementKind::import_statement) {
            throw std::runtime_error("module '" + module_name + "' (" + module_path.string() +
                                     ") can only contain imports, constants, functions, and "
                                     "types");
        }

        statements.push_back(std::move(statement));
    }

    return statements;
}

// Load every module referenced by a file's import statements, returning their
// resolved statements to prepend and filling `context` with this file's alias and
// selective-import rewrites. Validates selective symbols and alias conflicts.
std::vector<Statement> ModuleLoader::collect_imports(const std::vector<Statement>& statements,
                                                     const std::filesystem::path& importer_directory,
                                                     ImportContext& context) {
    std::vector<Statement> dependency_statements;
    std::unordered_set<std::string> imported_modules;

    for (const Statement& statement : statements) {
        if (statement.kind != StatementKind::import_statement) {
            continue;
        }

        const std::string& module = statement.name;
        std::vector<Statement> loaded = load_module(module, importer_directory);
        dependency_statements.insert(dependency_statements.end(), std::make_move_iterator(loaded.begin()),
                                     std::make_move_iterator(loaded.end()));

        // A plain module name must not collide with an alias bound elsewhere.
        const auto alias_collision = context.aliases.find(module);
        if (alias_collision != context.aliases.end() && alias_collision->second != module) {
            throw DiagnosticError(statement.location,
                                  "module '" + module + "' conflicts with an import alias of the same name");
        }
        imported_modules.insert(module);

        if (!statement.module_alias.empty()) {
            const std::string& alias = statement.module_alias;
            const auto existing = context.aliases.find(alias);
            if (existing != context.aliases.end()) {
                throw DiagnosticError(statement.location, "import alias '" + alias + "' is already bound to module '" +
                                                              existing->second + "'");
            }
            if (alias != module && imported_modules.contains(alias)) {
                throw DiagnosticError(statement.location,
                                      "import alias '" + alias + "' conflicts with imported module '" + alias + "'");
            }

            context.aliases.emplace(alias, module);
        }

        for (const std::string& symbol : statement.import_symbols) {
            // Skip the export check on a not-yet-finished module (a cyclic import);
            // otherwise a real typo is reported precisely.
            const auto exports = module_exports_.find(module);
            if (exports != module_exports_.end() && !exports->second.contains(symbol)) {
                throw DiagnosticError(statement.location, "module '" + module + "' does not export '" + symbol + "'");
            }

            const std::string qualified = module + "." + symbol;
            const auto existing = context.selective.find(symbol);
            if (existing != context.selective.end() && existing->second != qualified) {
                throw DiagnosticError(statement.location,
                                      "symbol '" + symbol + "' is imported from more than one module");
            }

            context.selective.emplace(symbol, qualified);
        }
    }

    return dependency_statements;
}

// Drop `module` declarations, canonicalize import statements to plain
// `import <module>;`, and rewrite every other statement's references through the
// alias / selective-import maps.
std::vector<Statement> ModuleLoader::rewrite_file(std::vector<Statement>& statements,
                                                  const ImportContext& context) const {
    std::vector<Statement> result;
    result.reserve(statements.size());
    for (Statement& statement : statements) {
        if (statement.kind == StatementKind::module_declaration) {
            continue;
        }

        if (statement.kind == StatementKind::import_statement) {
            statement.module_alias.clear();
            statement.import_symbols.clear();
            result.push_back(std::move(statement));
            continue;
        }

        apply_import_context(statement, context);
        result.push_back(std::move(statement));
    }

    return result;
}

// Record the exported top-level members of a freshly qualified module so later
// `from <module> import <symbol>` directives can be validated.
void ModuleLoader::record_module_exports(const std::string& module_name, const std::vector<Statement>& statements) {
    const std::string prefix = module_name + ".";
    auto& exports = module_exports_[module_name];
    for (const Statement& statement : statements) {
        if (!statement.exported ||
            (statement.kind != StatementKind::function && statement.kind != StatementKind::const_statement &&
             statement.kind != StatementKind::struct_statement && statement.kind != StatementKind::enum_statement &&
             statement.kind != StatementKind::contract_statement &&
             statement.kind != StatementKind::type_alias_statement)) {
            continue;
        }

        if (statement.name.rfind(prefix, 0) == 0) {
            exports.insert(statement.name.substr(prefix.size()));
        }

        if (statement.kind == StatementKind::enum_statement) {
            for (const Parameter& variant : statement.parameters) {
                if (variant.name.rfind(prefix, 0) == 0) {
                    exports.insert(variant.name.substr(prefix.size()));
                }
            }
        }
    }
}

std::filesystem::path ModuleLoader::find_module(const std::string& module_name,
                                                const std::filesystem::path& importer_directory) const {
    if (module_name.empty() || is_relative_to_parent(std::filesystem::path(module_name))) {
        throw std::runtime_error("invalid module name '" + module_name + "'");
    }

    const std::filesystem::path module_path = std::filesystem::path(module_name).replace_extension(".dn");

    // Standard library modules form a reserved namespace: they always resolve to
    // the search paths, and a same-named local file is rejected rather than
    // silently shadowing (or being shadowed by) the standard library.
    std::filesystem::path stdlib_match;
    for (const std::filesystem::path& search_path : search_paths_) {
        const std::filesystem::path candidate = search_path / module_path;
        if (std::filesystem::exists(candidate)) {
            stdlib_match = candidate;
            break;
        }
    }

    // Local modules resolve relative to the importing file, then the working
    // directory.
    std::filesystem::path local_match;
    for (const std::filesystem::path& directory : {importer_directory, std::filesystem::current_path()}) {
        if (directory.empty()) {
            continue;
        }

        const std::filesystem::path candidate = directory / module_path;
        if (std::filesystem::exists(candidate)) {
            local_match = candidate;
            break;
        }
    }

    if (!stdlib_match.empty() && !local_match.empty() && !std::filesystem::equivalent(stdlib_match, local_match)) {
        throw std::runtime_error("module '" + module_name + "' is a standard library module; the local file '" +
                                 local_match.string() +
                                 "' shadows it — rename the local module to avoid the collision");
    }

    if (!local_match.empty()) {
        return local_match;
    }

    if (!stdlib_match.empty()) {
        return stdlib_match;
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
    std::unordered_set<std::string> local_type_aliases;
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

        if (statement.kind == StatementKind::type_alias_statement) {
            local_type_aliases.insert(statement.name);
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
             statement.kind == StatementKind::contract_statement ||
             statement.kind == StatementKind::type_alias_statement) &&
            statement.exported) {
            has_explicit_exports = true;
        }
    }

    for (Statement& statement : program.statements) {
        if (statement.kind == StatementKind::const_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::function) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
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
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::enum_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
            for (Parameter& variant : statement.parameters) {
                variant.name = module_name + "." + variant.name;
            }
        }

        if (statement.kind == StatementKind::contract_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }

        if (statement.kind == StatementKind::type_alias_statement) {
            qualify_statement(statement, module_name, local_functions, local_constants, local_structs,
                              local_type_aliases, local_contracts);
            if (!has_explicit_exports) {
                statement.exported = true;
            }
            statement.name = module_name + "." + statement.name;
        }
    }
}

void ModuleLoader::qualify_type_annotation(TypeAnnotation& annotation, const std::string& module_name,
                                           const std::unordered_set<std::string>& local_structs,
                                           const std::unordered_set<std::string>& local_type_aliases) const {
    if (!annotation.has_type) {
        return;
    }

    qualify_type(annotation.type, module_name, local_structs, local_type_aliases);
}

void ModuleLoader::qualify_type(Type& type, const std::string& module_name,
                                const std::unordered_set<std::string>& local_structs,
                                const std::unordered_set<std::string>& local_type_aliases) const {
    if (type.kind == ValueType::array_type && type.element != nullptr) {
        qualify_type(*type.element, module_name, local_structs, local_type_aliases);
        return;
    }

    for (Type& argument : type.arguments) {
        qualify_type(argument, module_name, local_structs, local_type_aliases);
    }

    if (type.kind == ValueType::generic_type &&
        (local_structs.contains(type.name) || local_type_aliases.contains(type.name))) {
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
                                     const std::unordered_set<std::string>& local_type_aliases,
                                     const std::unordered_set<std::string>& local_contracts) const {
    qualify_generic_parameters(statement.generic_parameters, module_name, local_contracts);
    qualify_contracts(statement.contracts, module_name, local_contracts);
    qualify_type_annotation(statement.type, module_name, local_structs, local_type_aliases);
    for (Parameter& parameter : statement.parameters) {
        qualify_type_annotation(parameter.type, module_name, local_structs, local_type_aliases);
        if (parameter.default_value != nullptr) {
            qualify_expression(*parameter.default_value, module_name, local_functions, local_constants, local_structs,
                               local_type_aliases);
        }
    }

    if (statement.expression != nullptr) {
        qualify_expression(*statement.expression, module_name, local_functions, local_constants, local_structs,
                           local_type_aliases);
    }

    for (std::unique_ptr<Expression>& argument : statement.arguments) {
        qualify_expression(*argument, module_name, local_functions, local_constants, local_structs, local_type_aliases);
    }

    for (Statement& child : statement.body) {
        qualify_statement(child, module_name, local_functions, local_constants, local_structs, local_type_aliases,
                          local_contracts);
    }

    for (Statement& child : statement.else_body) {
        qualify_statement(child, module_name, local_functions, local_constants, local_structs, local_type_aliases,
                          local_contracts);
    }

    if (statement.initializer != nullptr) {
        qualify_statement(*statement.initializer, module_name, local_functions, local_constants, local_structs,
                          local_type_aliases, local_contracts);
    }

    if (statement.increment != nullptr) {
        qualify_statement(*statement.increment, module_name, local_functions, local_constants, local_structs,
                          local_type_aliases, local_contracts);
    }
}

void ModuleLoader::qualify_expression(Expression& expression, const std::string& module_name,
                                      const std::unordered_set<std::string>& local_functions,
                                      const std::unordered_set<std::string>& local_constants,
                                      const std::unordered_set<std::string>& local_structs,
                                      const std::unordered_set<std::string>& local_type_aliases) const {
    qualify_type_annotation(expression.type, module_name, local_structs, local_type_aliases);

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
        qualify_expression(*expression.left, module_name, local_functions, local_constants, local_structs,
                           local_type_aliases);
    }

    if (expression.right != nullptr) {
        qualify_expression(*expression.right, module_name, local_functions, local_constants, local_structs,
                           local_type_aliases);
    }

    for (std::unique_ptr<Expression>& argument : expression.arguments) {
        if (argument != nullptr) {
            qualify_expression(*argument, module_name, local_functions, local_constants, local_structs,
                               local_type_aliases);
        }
    }
}

void ModuleLoader::apply_import_context(Statement& statement, const ImportContext& context) const {
    apply_import_context_type_annotation(statement.type, context);
    for (Type& contract : statement.contracts) {
        apply_import_context_type(contract, context);
    }

    for (Parameter& parameter : statement.parameters) {
        apply_import_context_type_annotation(parameter.type, context);
        if (parameter.default_value != nullptr) {
            apply_import_context_expression(*parameter.default_value, context);
        }
    }

    if (statement.expression != nullptr) {
        apply_import_context_expression(*statement.expression, context);
    }

    if (statement.target != nullptr) {
        apply_import_context_expression(*statement.target, context);
    }

    for (std::unique_ptr<Expression>& argument : statement.arguments) {
        if (argument != nullptr) {
            apply_import_context_expression(*argument, context);
        }
    }

    for (Statement& child : statement.body) {
        apply_import_context(child, context);
    }

    for (Statement& child : statement.else_body) {
        apply_import_context(child, context);
    }

    if (statement.initializer != nullptr) {
        apply_import_context(*statement.initializer, context);
    }

    if (statement.increment != nullptr) {
        apply_import_context(*statement.increment, context);
    }
}

void ModuleLoader::apply_import_context_expression(Expression& expression, const ImportContext& context) const {
    apply_import_context_type_annotation(expression.type, context);

    // Rewrite a name through the import maps: an identifier head (`alias.member`),
    // a selectively imported constant / free-function call / record literal, or an
    // alias-qualified record literal such as `geo.Point`.
    if (expression.kind == ExpressionKind::identifier || expression.kind == ExpressionKind::call ||
        expression.kind == ExpressionKind::struct_literal) {
        if (const auto alias = context.aliases.find(expression.lexeme); alias != context.aliases.end()) {
            expression.lexeme = alias->second;
        } else if (const auto selective = context.selective.find(expression.lexeme);
                   selective != context.selective.end()) {
            expression.lexeme = selective->second;
        } else if (const std::size_t dot = expression.lexeme.find('.'); dot != std::string::npos) {
            if (const auto prefix = context.aliases.find(expression.lexeme.substr(0, dot));
                prefix != context.aliases.end()) {
                expression.lexeme = prefix->second + expression.lexeme.substr(dot);
            }
        }
    }

    if (expression.left != nullptr) {
        apply_import_context_expression(*expression.left, context);
    }

    if (expression.right != nullptr) {
        apply_import_context_expression(*expression.right, context);
    }

    for (std::unique_ptr<Expression>& argument : expression.arguments) {
        if (argument != nullptr) {
            apply_import_context_expression(*argument, context);
        }
    }
}

void ModuleLoader::apply_import_context_type_annotation(TypeAnnotation& annotation,
                                                        const ImportContext& context) const {
    if (!annotation.has_type) {
        return;
    }

    apply_import_context_type(annotation.type, context);
}

void ModuleLoader::apply_import_context_type(Type& type, const ImportContext& context) const {
    if (type.element != nullptr) {
        apply_import_context_type(*type.element, context);
    }

    for (Type& argument : type.arguments) {
        apply_import_context_type(argument, context);
    }

    // A selectively imported bare type name becomes fully qualified.
    if (const auto selective = context.selective.find(type.name); selective != context.selective.end()) {
        type.name = selective->second;
        return;
    }

    // An `alias.Type` reference has its alias head replaced by the real module.
    const std::size_t dot = type.name.find('.');
    if (dot != std::string::npos) {
        const auto alias = context.aliases.find(type.name.substr(0, dot));
        if (alias != context.aliases.end()) {
            type.name = alias->second + type.name.substr(dot);
        }
    }
}

} // namespace dune
