#pragma once

#include "ast/ast.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace dune {

class ModuleLoader {
public:
    ModuleLoader();
    explicit ModuleLoader(std::vector<std::filesystem::path> search_paths);

    Program resolve(Program program, const std::filesystem::path& source_directory = {});

private:
    std::vector<Statement> load_module(const std::string& module_name, const std::filesystem::path& importer_directory);
    std::filesystem::path find_module(const std::string& module_name,
                                      const std::filesystem::path& importer_directory) const;
    Program parse_file(const std::filesystem::path& path) const;
    void qualify_module_program(Program& program, const std::string& module_name) const;
    void qualify_statement(Statement& statement, const std::string& module_name,
                           const std::unordered_set<std::string>& local_functions,
                           const std::unordered_set<std::string>& local_constants,
                           const std::unordered_set<std::string>& local_structs,
                           const std::unordered_set<std::string>& local_type_aliases,
                           const std::unordered_set<std::string>& local_contracts) const;
    void qualify_expression(Expression& expression, const std::string& module_name,
                            const std::unordered_set<std::string>& local_functions,
                            const std::unordered_set<std::string>& local_constants,
                            const std::unordered_set<std::string>& local_structs,
                            const std::unordered_set<std::string>& local_type_aliases) const;
    void qualify_generic_parameters(std::vector<GenericParameter>& parameters, const std::string& module_name,
                                    const std::unordered_set<std::string>& local_contracts) const;
    void qualify_contracts(std::vector<Type>& contracts, const std::string& module_name,
                           const std::unordered_set<std::string>& local_contracts) const;
    void qualify_type_annotation(TypeAnnotation& annotation, const std::string& module_name,
                                 const std::unordered_set<std::string>& local_structs,
                                 const std::unordered_set<std::string>& local_type_aliases) const;
    void qualify_type(Type& type, const std::string& module_name, const std::unordered_set<std::string>& local_structs,
                      const std::unordered_set<std::string>& local_type_aliases) const;

    std::vector<std::filesystem::path> search_paths_;
    std::unordered_set<std::string> loaded_modules_;
};

} // namespace dune
