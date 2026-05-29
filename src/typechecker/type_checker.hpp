#pragma once

#include "ast/ast.hpp"

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dune {

class TypeChecker {
public:
    struct StructField {
        std::string name;
        Type type;
        SourceLocation location;
        bool exported = false;
    };

    struct StructMethod {
        std::string name;
        std::vector<Type> parameters;
        Type return_type;
        SourceLocation location;
        bool exported = false;
        bool is_constructor = false;
    };

    struct StructDefinition {
        std::string name;
        std::vector<GenericParameter> generic_parameters;
        std::vector<StructField> fields;
        std::vector<Type> contracts;
        std::vector<StructMethod> methods;
        std::unordered_map<std::string, std::size_t> field_indices;
        SourceLocation location;
    };

    struct ContractMethod {
        std::string name;
        std::vector<Type> parameters;
        Type return_type;
        SourceLocation location;
    };

    struct ContractDefinition {
        std::string name;
        std::vector<ContractMethod> methods;
        SourceLocation location;
    };

    struct EnumVariant {
        std::string name;
        bool has_payload = false;
        Type payload_type;
        std::size_t tag = 0;
        SourceLocation location;
    };

    struct EnumDefinition {
        std::string name;
        std::vector<GenericParameter> generic_parameters;
        std::vector<EnumVariant> variants;
        std::unordered_map<std::string, std::size_t> variant_indices;
        SourceLocation location;
    };

    struct VariantResolution {
        std::string enum_name;
        std::string variant_name;
        std::size_t tag = 0;
        bool has_payload = false;
        Type payload_type;
        std::string binding_name;
        bool binds_payload = false;
    };

    void check(const Program& program);
    const std::unordered_map<const Expression*, Type>& expression_types() const;
    const std::unordered_map<const Expression*, std::string>& resolved_calls() const;
    const std::unordered_map<const Expression*, VariantResolution>& resolved_variants() const;
    const std::deque<Statement>& instantiated_functions() const;
    const std::unordered_map<std::string, StructDefinition>& structs() const;
    const std::unordered_map<std::string, EnumDefinition>& enums() const;

private:
    struct FunctionSignature {
        std::string name;
        std::string key;
        std::vector<Type> parameters;
        Type return_type;
        SourceLocation location;
    };

    struct VariableBinding {
        Type type;
        bool constant = false;
    };

    void declare_struct(const Statement& statement);
    void define_struct(const Statement& statement);
    void declare_enum(const Statement& statement);
    void define_enum(const Statement& statement);
    void declare_contract(const Statement& statement);
    void define_contract(const Statement& statement);
    void validate_constructor(const Statement& record, const Statement& method,
                              const std::unordered_set<std::string>& generic_names) const;
    void validate_contract_implementations(const Statement& statement) const;
    void collect_function(const Statement& statement);
    void collect_generic_function(const Statement& statement);
    void check_function(const Statement& statement);
    void check_statement(const Statement& statement);
    void check_statements(const std::vector<Statement>& statements);
    Type check_assignment_target(const Expression& target, SourceLocation location);
    Type check_member_assignment_target(const Expression& target, SourceLocation location);
    Type check_expression(const Expression& expression, const TypeAnnotation& expected = {});
    Type check_binary_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_when_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_call_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_method_call_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_constructor_call_expression(const Expression& expression, const std::string& record_name,
                                           const TypeAnnotation& expected);
    Type check_member_expression(const Expression& expression, const TypeAnnotation& expected);
    Type check_unary_expression(const Expression& expression);
    Type check_cast_expression(const Expression& expression);
    Type check_array_literal(const Expression& expression, const TypeAnnotation& expected);
    Type check_tuple_literal(const Expression& expression, const TypeAnnotation& expected);
    Type check_struct_literal(const Expression& expression, const TypeAnnotation& expected);
    Type check_index_expression(const Expression& expression);
    Type check_slice_expression(const Expression& expression);
    Type check_function_call(const Expression& expression, const std::string& name,
                             const std::vector<std::unique_ptr<Expression>>& arguments, SourceLocation location,
                             const TypeAnnotation& expected = {});
    Type check_variant_constructor(const Expression& expression, const std::string& name,
                                   const std::vector<std::unique_ptr<Expression>>& arguments, SourceLocation location,
                                   const TypeAnnotation& expected);
    Type check_receiver_method_call(const Expression& expression, const TypeAnnotation& expected);
    Type check_array_method_call(const Type& receiver, const Expression& expression);
    Type check_text_method_call(const Type& receiver, const Expression& expression);

    bool statement_returns(const Statement& statement) const;
    bool statements_return(const std::vector<Statement>& statements) const;

    Type annotation_or_default(const TypeAnnotation& annotation) const;
    Type annotation_or_default(const TypeAnnotation& annotation,
                               const std::unordered_set<std::string>& generic_parameters) const;
    Type normalize_type(const Type& type, const std::unordered_set<std::string>& generic_parameters = {}) const;
    bool same_type(const Type& left, const Type& right) const;
    bool is_signed_type(ValueType type) const;
    bool is_integer_type(ValueType type) const;
    bool is_unsigned_type(ValueType type) const;
    bool is_real_type(ValueType type) const;
    bool is_numeric_type(const Type& type) const;
    bool is_comparable_type(const Type& type) const;
    bool is_ordered_type(const Type& type) const;
    bool is_cast_allowed(const Type& source, const Type& target) const;
    bool can_coerce_integer_literal(const Expression& expression, const Type& target) const;
    bool is_numeric_literal(const Expression& expression) const;
    bool type_satisfies_bound(const Type& type, const std::string& bound, SourceLocation location) const;
    bool type_satisfies_contract_bound(const Type& type, const std::string& bound) const;
    bool collect_generic_constraints(const Type& pattern, const Type& actual,
                                     std::unordered_map<std::string, std::vector<std::pair<Type, bool>>>& constraints,
                                     bool preferred) const;
    const EnumVariant* find_enum_variant(const EnumDefinition& definition, const std::string& name) const;
    const EnumDefinition* find_expected_enum(const TypeAnnotation& expected) const;
    Type substitute_enum_payload(const EnumDefinition& definition, const Type& enum_type,
                                 const EnumVariant& variant) const;
    bool is_variant_name_for_expected_enum(const std::string& name, const TypeAnnotation& expected) const;
    VariantResolution resolve_variant_pattern(const Expression& pattern, const Type& subject);
    Type check_enum_when_expression(const Expression& expression, const Type& subject, const TypeAnnotation& expected);
    Type check_record_when_expression(const Expression& expression, const Type& subject,
                                      const TypeAnnotation& expected);
    Type check_tuple_when_expression(const Expression& expression, const Type& subject, const TypeAnnotation& expected);
    void check_tuple_destructuring_assignment(const Expression& target, const Expression& value);
    void bind_record_pattern(const Expression& pattern, const Type& subject);
    void bind_tuple_pattern(const Expression& pattern, const Type& subject);
    void check_integer_literal_range(const Expression& expression, const Type& target) const;
    unsigned long long max_integer_literal(ValueType target) const;
    Type coerce_numeric_literal(const Expression& expression, const Type& actual, const Type& target);
    void expect_type(const Type& expected, const Type& actual, SourceLocation location) const;
    void expect_imported_module(const std::string& module, SourceLocation location) const;
    void expect_exported_member(const std::string& module, const std::string& member, SourceLocation location) const;
    void expect_public_field(const Type& receiver, const StructField& field, SourceLocation location) const;
    void expect_public_method(const Type& receiver, const std::string& method, SourceLocation location) const;
    void collect_known_module(const std::string& name);
    void collect_module_export(const Statement& statement);
    bool is_qualified_module_name(const std::string& name) const;
    bool is_known_module(const std::string& module) const;
    bool is_external_record_access(const std::string& record_name) const;
    std::string current_access_module() const;
    std::string diagnostic(SourceLocation location, const std::string& message) const;
    void reset_scopes();
    void push_scope();
    void pop_scope();
    VariableBinding* find_binding(const std::string& name);
    const VariableBinding* find_binding(const std::string& name) const;
    bool has_visible_constant(const std::string& name) const;
    void declare_binding(const std::string& name, const Type& type, bool constant, SourceLocation location);

    const FunctionSignature& resolve_overload(const std::string& name, const std::vector<const Expression*>& arguments,
                                              SourceLocation location, const TypeAnnotation& expected);
    const FunctionSignature* try_resolve_overload(const std::string& name,
                                                  const std::vector<const Expression*>& arguments,
                                                  SourceLocation location, const TypeAnnotation& expected);
    const FunctionSignature& find_function_by_key(const std::string& key, SourceLocation location) const;
    const FunctionSignature& instantiate_generic_function(const Statement& statement,
                                                          const std::unordered_map<std::string, Type>& substitutions,
                                                          SourceLocation location);
    std::string instantiation_trace(const Statement& statement,
                                    const std::unordered_map<std::string, Type>& substitutions) const;

    std::unordered_map<std::string, StructDefinition> structs_;
    std::unordered_map<std::string, EnumDefinition> enums_;
    std::unordered_map<std::string, ContractDefinition> contracts_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, std::vector<std::string>> overloads_;
    std::unordered_map<std::string, std::vector<const Statement*>> generic_overloads_;
    std::unordered_set<std::string> instantiated_function_keys_;
    std::deque<Statement> instantiated_functions_;
    std::unordered_map<std::string, std::string> instantiated_function_traces_;
    std::vector<std::unordered_map<std::string, VariableBinding>> scopes_;
    std::unordered_map<std::string, Type> global_constants_;
    std::unordered_map<std::string, std::unordered_set<std::string>> module_exports_;
    std::unordered_set<std::string> known_modules_;
    std::unordered_map<const Expression*, Type> expression_types_;
    std::unordered_map<const Expression*, std::string> resolved_calls_;
    std::unordered_map<const Expression*, VariantResolution> resolved_variants_;
    std::unordered_set<std::string> imports_;
    const FunctionSignature* current_function_ = nullptr;
    std::string current_module_;
    std::size_t loop_depth_ = 0;
};

std::string type_name(ValueType type);
std::string type_name(const Type& type);
Type make_type(ValueType type);
Type make_array_type(Type element);
Type make_tuple_type(std::vector<Type> elements);
Type make_struct_type(std::string name);
Type make_struct_type(std::string name, std::vector<Type> arguments);
Type make_enum_type(std::string name);
Type make_enum_type(std::string name, std::vector<Type> arguments);
std::string type_key(const Type& type);
std::string function_key(const std::string& name, const std::vector<Type>& parameters);

} // namespace dune
