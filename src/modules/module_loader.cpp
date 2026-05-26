#include "module_loader.hpp"

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

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
