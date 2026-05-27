#include "llvm_ir_generator.hpp"

#include "typechecker/type_checker.hpp"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

bool is_unsigned_type(ValueType type) {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type || type == ValueType::usize_type;
}

bool is_signed_type(ValueType type) {
    return type == ValueType::int_type || type == ValueType::i8_type || type == ValueType::i16_type ||
           type == ValueType::i32_type || type == ValueType::i64_type || type == ValueType::isize_type;
}

bool is_integer_type(ValueType type) {
    return is_signed_type(type) || is_unsigned_type(type);
}

bool is_real_type(ValueType type) {
    return type == ValueType::real32_type || type == ValueType::real_type;
}

std::string real_literal(const std::string& lexeme, const Type& type) {
    if (type.kind == ValueType::real32_type) {
        const double rounded = static_cast<double>(static_cast<float>(std::stod(lexeme)));
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(rounded);
        std::ostringstream output;
        output << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << bits;
        return output.str();
    }

    std::ostringstream output;
    output << std::scientific << std::setprecision(16) << std::stod(lexeme);
    return output.str();
}

std::size_t align_to(std::size_t value, std::size_t alignment) {
    if (alignment <= 1) {
        return value;
    }

    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + alignment - remainder;
}

} // namespace

void LlvmIrGenerator::generate(const Program& program, std::ostream& output) {
    TypeChecker checker;
    checker.check(program);

    expression_types_ = checker.expression_types();
    resolved_calls_ = checker.resolved_calls();
    resolved_variants_ = checker.resolved_variants();
    const auto& instantiated_functions = checker.instantiated_functions();
    functions_.clear();
    structs_.clear();
    enums_.clear();
    global_constants_.clear();
    string_globals_.clear();
    loop_stack_.clear();
    collect_structs(checker.structs());
    collect_enums(checker.enums());
    collect_global_constants(program);
    collect_functions(program.statements);
    for (const Statement& statement : instantiated_functions) {
        collect_function(statement);
    }
    register_count_ = 0;
    label_count_ = 0;
    string_literal_count_ = 0;
    temporary_count_ = 0;

    std::ostringstream body;

    locals_.clear();
    temporary_count_ = 0;
    current_return_type_ = make_type(ValueType::int_type);
    body << "define i32 @main() {\n";
    emit_statements(program.statements, body);
    body << "  call void @dune_free_all()\n";
    body << "  ret i32 0\n";
    body << "}\n\n";

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function && statement.generic_parameters.empty()) {
            emit_function(statement, body);
        }
    }
    for (const Statement& statement : instantiated_functions) {
        emit_function(statement, body);
    }

    output << "@.dune_fmt_sint = private unnamed_addr constant [6 x i8] c\"%lld\\0A\\00\"\n";
    output << "@.dune_fmt_uint = private unnamed_addr constant [6 x i8] c\"%llu\\0A\\00\"\n";
    output << "@.dune_fmt_real = private unnamed_addr constant [7 x i8] c\"%.15g\\0A\\00\"\n";
    output << "@.dune_fmt_glyph = private unnamed_addr constant [4 x i8] c\"%c\\0A\\00\"\n";
    output << "@.dune_fmt_text = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\"\n";
    output << "@.dune_panic_array_index = private unnamed_addr constant [27 x i8] c\"array index out of "
              "bounds\\0A\\00\"\n";
    output
        << "@.dune_panic_text_index = private unnamed_addr constant [26 x i8] c\"text index out of bounds\\0A\\00\"\n";
    output << "@.dune_panic_slice = private unnamed_addr constant [27 x i8] c\"slice bound out of bounds\\0A\\00\"\n";
    output << "@.dune_panic_array_pop = private unnamed_addr constant [29 x i8] c\"cannot pop from empty "
              "array\\0A\\00\"\n";
    output << "@.dune_allocs = internal global ptr null\n";
    output << "@.dune_alloc_count = internal global i64 0\n";
    output << "@.dune_alloc_capacity = internal global i64 0\n";
    for (const std::string& global : string_globals_) {
        output << global;
    }

    output << "declare i32 @printf(ptr, ...)\n";
    output << "declare i32 @strcmp(ptr, ptr)\n\n";
    output << "declare i64 @strlen(ptr)\n";
    output << "declare ptr @strstr(ptr, ptr)\n";
    output << "declare i32 @strncmp(ptr, ptr, i64)\n\n";
    output << "declare ptr @malloc(i64)\n";
    output << "declare ptr @realloc(ptr, i64)\n\n";
    output << "declare void @free(ptr)\n\n";
    output << "declare ptr @memcpy(ptr, ptr, i64)\n\n";
    output << "declare void @exit(i32)\n\n";
    emit_extern_declarations(output);
    emit_memory_runtime(output);
    output << body.str();
}

void LlvmIrGenerator::collect_functions(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind == StatementKind::function && statement.generic_parameters.empty()) {
            collect_function(statement);
        }
    }
}

void LlvmIrGenerator::collect_global_constants(const Program& program) {
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::const_statement && statement.name.find('.') != std::string::npos) {
            global_constants_.push_back(&statement);
        }
    }
}

void LlvmIrGenerator::collect_function(const Statement& statement) {
    FunctionSignature signature;
    signature.return_type =
        statement.type.has_type ? normalize_type(statement.type.type) : make_type(ValueType::int_type);
    signature.extern_symbol = statement.extern_symbol.empty() ? statement.name : statement.extern_symbol;
    signature.is_extern = statement.is_extern;

    for (const Parameter& parameter : statement.parameters) {
        signature.parameters.push_back(parameter.type.has_type ? normalize_type(parameter.type.type)
                                                               : make_type(ValueType::int_type));
    }

    functions_.emplace(function_key(statement.name, signature.parameters), std::move(signature));
}

void LlvmIrGenerator::collect_structs(const std::unordered_map<std::string, TypeChecker::StructDefinition>& structs) {
    for (const auto& [name, definition] : structs) {
        StructLayout layout;
        layout.generic_parameters = definition.generic_parameters;
        for (const TypeChecker::StructField& field : definition.fields) {
            layout.field_indices.emplace(field.name, layout.fields.size());
            layout.fields.push_back(Parameter{field.name, TypeAnnotation{true, field.type}, field.location});
        }

        structs_.emplace(name, std::move(layout));
    }
}

void LlvmIrGenerator::collect_enums(const std::unordered_map<std::string, TypeChecker::EnumDefinition>& enums) {
    for (const auto& [name, definition] : enums) {
        (void)definition;
        enums_.insert(name);
    }
}

void LlvmIrGenerator::emit_function(const Statement& statement, std::ostream& output) {
    if (statement.is_extern) {
        return;
    }

    std::vector<Type> parameters;
    parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameters.push_back(parameter.type.has_type ? normalize_type(parameter.type.type)
                                                     : make_type(ValueType::int_type));
    }

    const std::string key = function_key(statement.name, parameters);
    const auto signature = functions_.find(key);
    if (signature == functions_.end()) {
        throw std::runtime_error("undefined function '" + statement.name + "'");
    }

    locals_.clear();
    current_return_type_ = signature->second.return_type;

    output << "define " << llvm_type(signature->second.return_type) << " " << function_name(key) << "(";
    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }

        output << llvm_type(signature->second.parameters[index]) << " %arg" << index;
    }
    output << ") {\n";

    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        const Parameter& parameter = statement.parameters[index];
        const Type type = signature->second.parameters[index];
        const std::string pointer = next_register();
        output << "  " << pointer << " = alloca " << llvm_type(type) << '\n';
        output << "  store " << llvm_type(type) << " %arg" << index << ", ptr " << pointer << '\n';
        locals_.emplace(parameter.name, Local{pointer, type});
    }

    emit_global_constants(output);

    const bool terminated = emit_statements(statement.body, output);
    if (!terminated) {
        output << "  ret " << llvm_type(signature->second.return_type) << ' '
               << default_value(signature->second.return_type) << '\n';
    }

    output << "}\n\n";
}

void LlvmIrGenerator::emit_extern_declarations(std::ostream& output) {
    std::unordered_set<std::string> declarations;
    for (const auto& [key, signature] : functions_) {
        (void)key;
        if (!signature.is_extern) {
            continue;
        }

        std::ostringstream declaration;
        declaration << "declare " << llvm_type(signature.return_type) << ' ' << extern_function_name(signature) << '(';
        for (std::size_t index = 0; index < signature.parameters.size(); ++index) {
            if (index > 0) {
                declaration << ", ";
            }

            declaration << llvm_type(signature.parameters[index]);
        }
        declaration << ")\n";

        declarations.insert(declaration.str());
    }

    for (const std::string& declaration : declarations) {
        output << declaration;
    }

    if (!declarations.empty()) {
        output << '\n';
    }
}

void LlvmIrGenerator::emit_memory_runtime(std::ostream& output) {
    output << "define void @dune_track_alloc(ptr %value) {\n";
    output << "entry:\n";
    output << "  %is_null = icmp eq ptr %value, null\n";
    output << "  br i1 %is_null, label %done, label %non_null\n";
    output << "non_null:\n";
    output << "  %count = load i64, ptr @.dune_alloc_count\n";
    output << "  %capacity = load i64, ptr @.dune_alloc_capacity\n";
    output << "  %full = icmp eq i64 %count, %capacity\n";
    output << "  br i1 %full, label %grow, label %store\n";
    output << "grow:\n";
    output << "  %zero = icmp eq i64 %capacity, 0\n";
    output << "  %doubled = mul i64 %capacity, 2\n";
    output << "  %next_capacity = select i1 %zero, i64 16, i64 %doubled\n";
    output << "  %bytes = mul i64 %next_capacity, 8\n";
    output << "  %old_list = load ptr, ptr @.dune_allocs\n";
    output << "  %next_list = call ptr @realloc(ptr %old_list, i64 %bytes)\n";
    output << "  store ptr %next_list, ptr @.dune_allocs\n";
    output << "  store i64 %next_capacity, ptr @.dune_alloc_capacity\n";
    output << "  br label %store\n";
    output << "store:\n";
    output << "  %list = load ptr, ptr @.dune_allocs\n";
    output << "  %slot = getelementptr ptr, ptr %list, i64 %count\n";
    output << "  store ptr %value, ptr %slot\n";
    output << "  %next_count = add i64 %count, 1\n";
    output << "  store i64 %next_count, ptr @.dune_alloc_count\n";
    output << "  br label %done\n";
    output << "done:\n";
    output << "  ret void\n";
    output << "}\n\n";

    output << "define ptr @dune_alloc(i64 %size) {\n";
    output << "entry:\n";
    output << "  %value = call ptr @malloc(i64 %size)\n";
    output << "  call void @dune_track_alloc(ptr %value)\n";
    output << "  ret ptr %value\n";
    output << "}\n\n";

    output << "define ptr @dune_realloc(ptr %old, i64 %size) {\n";
    output << "entry:\n";
    output << "  %value = call ptr @realloc(ptr %old, i64 %size)\n";
    output << "  %count = load i64, ptr @.dune_alloc_count\n";
    output << "  br label %loop\n";
    output << "loop:\n";
    output << "  %index = phi i64 [ 0, %entry ], [ %next, %advance ]\n";
    output << "  %done_scan = icmp eq i64 %index, %count\n";
    output << "  br i1 %done_scan, label %track, label %check\n";
    output << "check:\n";
    output << "  %list = load ptr, ptr @.dune_allocs\n";
    output << "  %slot = getelementptr ptr, ptr %list, i64 %index\n";
    output << "  %current = load ptr, ptr %slot\n";
    output << "  %matched = icmp eq ptr %current, %old\n";
    output << "  br i1 %matched, label %update, label %advance\n";
    output << "update:\n";
    output << "  store ptr %value, ptr %slot\n";
    output << "  ret ptr %value\n";
    output << "advance:\n";
    output << "  %next = add i64 %index, 1\n";
    output << "  br label %loop\n";
    output << "track:\n";
    output << "  call void @dune_track_alloc(ptr %value)\n";
    output << "  ret ptr %value\n";
    output << "}\n\n";

    output << "define void @dune_free_all() {\n";
    output << "entry:\n";
    output << "  %count = load i64, ptr @.dune_alloc_count\n";
    output << "  %list = load ptr, ptr @.dune_allocs\n";
    output << "  br label %loop\n";
    output << "loop:\n";
    output << "  %index = phi i64 [ 0, %entry ], [ %next, %skip ]\n";
    output << "  %done = icmp eq i64 %index, %count\n";
    output << "  br i1 %done, label %after, label %body\n";
    output << "body:\n";
    output << "  %slot = getelementptr ptr, ptr %list, i64 %index\n";
    output << "  %value = load ptr, ptr %slot\n";
    output << "  %is_null = icmp eq ptr %value, null\n";
    output << "  br i1 %is_null, label %skip, label %free_value\n";
    output << "free_value:\n";
    output << "  call void @free(ptr %value)\n";
    output << "  br label %skip\n";
    output << "skip:\n";
    output << "  %next = add i64 %index, 1\n";
    output << "  br label %loop\n";
    output << "after:\n";
    output << "  call void @free(ptr %list)\n";
    output << "  store ptr null, ptr @.dune_allocs\n";
    output << "  store i64 0, ptr @.dune_alloc_count\n";
    output << "  store i64 0, ptr @.dune_alloc_capacity\n";
    output << "  ret void\n";
    output << "}\n\n";
}

void LlvmIrGenerator::emit_global_constants(std::ostream& output) {
    for (const Statement* statement : global_constants_) {
        const TypedValue value = emit_expression(*statement->expression, output);
        auto existing = locals_.find(statement->name);
        if (existing == locals_.end()) {
            const std::string pointer = next_register();
            output << "  " << pointer << " = alloca " << llvm_type(value.type) << '\n';
            existing = locals_.emplace(statement->name, Local{pointer, value.type}).first;
        }

        output << "  store " << llvm_type(value.type) << ' ' << value.name << ", ptr " << existing->second.pointer
               << '\n';
    }
}

bool LlvmIrGenerator::emit_statements(const std::vector<Statement>& statements, std::ostream& output) {
    for (const Statement& statement : statements) {
        if (emit_statement(statement, output)) {
            return true;
        }
    }

    return false;
}

bool LlvmIrGenerator::emit_statement(const Statement& statement, std::ostream& output) {
    switch (statement.kind) {
    case StatementKind::var:
    case StatementKind::const_statement: {
        const TypedValue value = emit_expression(*statement.expression, output);
        auto existing = locals_.find(statement.name);
        if (existing == locals_.end()) {
            const std::string pointer = next_register();
            output << "  " << pointer << " = alloca " << llvm_type(value.type) << '\n';
            existing = locals_.emplace(statement.name, Local{pointer, value.type}).first;
        }

        output << "  store " << llvm_type(value.type) << ' ' << value.name << ", ptr " << existing->second.pointer
               << '\n';
        return false;
    }
    case StatementKind::assign: {
        const auto local = locals_.find(statement.name);
        if (local == locals_.end()) {
            throw std::runtime_error("undefined variable '" + statement.name + "'");
        }

        const TypedValue value = emit_expression(*statement.expression, output);
        output << "  store " << llvm_type(value.type) << ' ' << value.name << ", ptr " << local->second.pointer << '\n';
        return false;
    }
    case StatementKind::print:
        emit_print(emit_expression(*statement.expression, output), output);
        return false;
    case StatementKind::block:
        return emit_statements(statement.body, output);
    case StatementKind::if_statement: {
        const TypedValue condition = emit_expression(*statement.expression, output);
        const std::string then_label = next_label("then");
        const std::string else_label = next_label("else");
        const std::string end_label = next_label("endif");
        output << "  br i1 " << condition.name << ", label %" << then_label << ", label %" << else_label << '\n';
        output << then_label << ":\n";
        const bool then_terminated = emit_statements(statement.body, output);
        if (!then_terminated) {
            output << "  br label %" << end_label << '\n';
        }

        output << else_label << ":\n";
        const bool else_terminated = emit_statements(statement.else_body, output);
        if (!else_terminated) {
            output << "  br label %" << end_label << '\n';
        }

        if (then_terminated && else_terminated && !statement.else_body.empty()) {
            return true;
        }

        output << end_label << ":\n";
        return false;
    }
    case StatementKind::while_statement: {
        const std::string condition_label = next_label("while_cond");
        const std::string body_label = next_label("while_body");
        const std::string end_label = next_label("while_end");
        output << "  br label %" << condition_label << '\n';
        output << condition_label << ":\n";
        const TypedValue condition = emit_expression(*statement.expression, output);
        output << "  br i1 " << condition.name << ", label %" << body_label << ", label %" << end_label << '\n';
        output << body_label << ":\n";
        loop_stack_.push_back(LoopLabels{end_label, condition_label});
        const bool body_terminated = emit_statements(statement.body, output);
        loop_stack_.pop_back();
        if (!body_terminated) {
            output << "  br label %" << condition_label << '\n';
        }

        output << end_label << ":\n";
        return false;
    }
    case StatementKind::for_statement: {
        if (statement.initializer != nullptr) {
            emit_statement(*statement.initializer, output);
        }

        const std::string condition_label = next_label("for_cond");
        const std::string body_label = next_label("for_body");
        const std::string increment_label = next_label("for_increment");
        const std::string end_label = next_label("for_end");
        output << "  br label %" << condition_label << '\n';
        output << condition_label << ":\n";
        const TypedValue condition = emit_expression(*statement.expression, output);
        output << "  br i1 " << condition.name << ", label %" << body_label << ", label %" << end_label << '\n';
        output << body_label << ":\n";
        loop_stack_.push_back(LoopLabels{end_label, increment_label});
        const bool body_terminated = emit_statements(statement.body, output);
        loop_stack_.pop_back();
        if (!body_terminated) {
            output << "  br label %" << increment_label << '\n';
        }

        output << increment_label << ":\n";
        if (statement.increment != nullptr) {
            emit_statement(*statement.increment, output);
        }
        output << "  br label %" << condition_label << '\n';
        output << end_label << ":\n";
        return false;
    }
    case StatementKind::break_statement:
        if (loop_stack_.empty()) {
            throw std::runtime_error("break statement outside loop");
        }

        output << "  br label %" << loop_stack_.back().break_label << '\n';
        return true;
    case StatementKind::continue_statement:
        if (loop_stack_.empty()) {
            throw std::runtime_error("continue statement outside loop");
        }

        output << "  br label %" << loop_stack_.back().continue_label << '\n';
        return true;
    case StatementKind::function:
    case StatementKind::impl_statement:
    case StatementKind::struct_statement:
    case StatementKind::enum_statement:
        return false;
    case StatementKind::return_statement: {
        if (statement.expression == nullptr) {
            output << "  ret " << llvm_type(current_return_type_) << ' ' << default_value(current_return_type_) << '\n';
            return true;
        }

        const TypedValue value = emit_expression(*statement.expression, output);
        output << "  ret " << llvm_type(current_return_type_) << ' ' << value.name << '\n';
        return true;
    }
    case StatementKind::expression_statement:
        emit_expression(*statement.expression, output);
        return false;
    case StatementKind::import_statement:
        return false;
    }

    return false;
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_expression(const Expression& expression, std::ostream& output) {
    const auto type = expression_types_.find(&expression);
    if (type == expression_types_.end()) {
        throw std::runtime_error("missing inferred expression type");
    }

    if (resolved_variants_.contains(&expression)) {
        return emit_variant_constructor(expression, output);
    }

    switch (expression.kind) {
    case ExpressionKind::identifier: {
        const auto local = locals_.find(expression.lexeme);
        if (local == locals_.end()) {
            throw std::runtime_error("undefined variable '" + expression.lexeme + "'");
        }

        const std::string value = next_register();
        output << "  " << value << " = load " << llvm_type(local->second.type) << ", ptr " << local->second.pointer
               << '\n';
        return TypedValue{value, local->second.type};
    }
    case ExpressionKind::number:
        if (is_real_type(type->second.kind)) {
            return TypedValue{real_literal(expression.lexeme, type->second), type->second};
        }

        return TypedValue{expression.lexeme, type->second};
    case ExpressionKind::floating:
        return TypedValue{real_literal(expression.lexeme, type->second), type->second};
    case ExpressionKind::character:
        return TypedValue{decode_glyph_literal(expression.lexeme), make_type(ValueType::glyph_type)};
    case ExpressionKind::string:
        return emit_text_literal(expression.lexeme);
    case ExpressionKind::boolean:
        return TypedValue{expression.lexeme == "true" ? "1" : "0", make_type(ValueType::bool_type)};
    case ExpressionKind::array:
        return emit_array_literal(expression, output);
    case ExpressionKind::struct_literal:
        return emit_struct_literal(expression, output);
    case ExpressionKind::index:
        return emit_index_expression(expression, output);
    case ExpressionKind::slice:
        return emit_slice_expression(expression, output);
    case ExpressionKind::member:
        return emit_member_expression(expression, output);
    case ExpressionKind::unary:
        return emit_unary_expression(expression, output);
    case ExpressionKind::cast:
        return emit_cast_expression(expression, output);
    case ExpressionKind::binary:
        return emit_binary_expression(expression, output);
    case ExpressionKind::match_expression:
        return emit_match_expression(expression, output);
    case ExpressionKind::call:
        return emit_call_expression(expression, output);
    case ExpressionKind::method_call:
        return emit_method_call_expression(expression, output);
    }

    throw std::runtime_error("unknown expression");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_binary_expression(const Expression& expression,
                                                                    std::ostream& output) {
    if (expression.lexeme == "&&" || expression.lexeme == "||") {
        return emit_logical_expression(expression, output);
    }

    const TypedValue left = emit_expression(*expression.left, output);
    const TypedValue right = emit_expression(*expression.right, output);
    const std::string result = next_register();

    if (expression.lexeme == "+" || expression.lexeme == "-" || expression.lexeme == "*" || expression.lexeme == "/" ||
        expression.lexeme == "%") {
        std::string op;
        if (is_real_type(left.type.kind)) {
            if (expression.lexeme == "+") {
                op = "fadd";
            } else if (expression.lexeme == "-") {
                op = "fsub";
            } else if (expression.lexeme == "*") {
                op = "fmul";
            } else if (expression.lexeme == "/") {
                op = "fdiv";
            } else {
                throw std::runtime_error("unknown real binary operator");
            }
        } else if (expression.lexeme == "/") {
            op = is_unsigned_type(left.type.kind) ? "udiv" : "sdiv";
        } else if (expression.lexeme == "%") {
            op = is_unsigned_type(left.type.kind) ? "urem" : "srem";
        } else if (expression.lexeme == "+") {
            op = "add";
        } else if (expression.lexeme == "-") {
            op = "sub";
        } else {
            op = "mul";
        }

        output << "  " << result << " = " << op << ' ' << llvm_type(left.type) << ' ' << left.name << ", " << right.name
               << '\n';
        return TypedValue{result, left.type};
    }

    if (left.type.kind == ValueType::text_type) {
        if (expression.lexeme != "==" && expression.lexeme != "!=") {
            throw std::runtime_error("unknown text binary operator");
        }

        const std::string comparison = next_register();
        output << "  " << comparison << " = call i32 @strcmp(ptr " << left.name << ", ptr " << right.name << ")\n";
        output << "  " << result << " = icmp " << (expression.lexeme == "==" ? "eq" : "ne") << " i32 " << comparison
               << ", 0\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    std::string op;
    if (expression.lexeme == "==") {
        op = is_real_type(left.type.kind) ? "oeq" : "eq";
    } else if (expression.lexeme == "!=") {
        op = is_real_type(left.type.kind) ? "one" : "ne";
    } else if (expression.lexeme == ">") {
        op = is_real_type(left.type.kind) ? "ogt" : (is_unsigned_type(left.type.kind) ? "ugt" : "sgt");
    } else if (expression.lexeme == ">=") {
        op = is_real_type(left.type.kind) ? "oge" : (is_unsigned_type(left.type.kind) ? "uge" : "sge");
    } else if (expression.lexeme == "<") {
        op = is_real_type(left.type.kind) ? "olt" : (is_unsigned_type(left.type.kind) ? "ult" : "slt");
    } else if (expression.lexeme == "<=") {
        op = is_real_type(left.type.kind) ? "ole" : (is_unsigned_type(left.type.kind) ? "ule" : "sle");
    } else {
        throw std::runtime_error("unknown binary operator");
    }

    output << "  " << result << " = " << (is_real_type(left.type.kind) ? "fcmp " : "icmp ") << op << ' '
           << llvm_type(left.type) << ' ' << left.name << ", " << right.name << '\n';
    return TypedValue{result, make_type(ValueType::bool_type)};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_logical_expression(const Expression& expression,
                                                                     std::ostream& output) {
    const TypedValue left = emit_expression(*expression.left, output);
    const std::string result_pointer = next_register();
    const std::string right_label = next_label("logical_right");
    const std::string short_label = next_label("logical_short");
    const std::string end_label = next_label("logical_end");

    output << "  " << result_pointer << " = alloca i1\n";
    if (expression.lexeme == "&&") {
        output << "  br i1 " << left.name << ", label %" << right_label << ", label %" << short_label << '\n';
        output << short_label << ":\n";
        output << "  store i1 0, ptr " << result_pointer << '\n';
    } else {
        output << "  br i1 " << left.name << ", label %" << short_label << ", label %" << right_label << '\n';
        output << short_label << ":\n";
        output << "  store i1 1, ptr " << result_pointer << '\n';
    }
    output << "  br label %" << end_label << '\n';

    output << right_label << ":\n";
    const TypedValue right = emit_expression(*expression.right, output);
    output << "  store i1 " << right.name << ", ptr " << result_pointer << '\n';
    output << "  br label %" << end_label << '\n';

    output << end_label << ":\n";
    const std::string result = next_register();
    output << "  " << result << " = load i1, ptr " << result_pointer << '\n';
    return TypedValue{result, make_type(ValueType::bool_type)};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_match_expression(const Expression& expression, std::ostream& output) {
    const TypedValue subject = emit_expression(*expression.left, output);
    const auto inferred = expression_types_.find(&expression);
    if (inferred == expression_types_.end()) {
        throw std::runtime_error("missing inferred case type");
    }

    const std::string subject_pointer = next_register();
    const std::string result_pointer = next_register();
    const std::string end_label = next_label("case_end");
    output << "  " << subject_pointer << " = alloca " << llvm_type(subject.type) << '\n';
    output << "  store " << llvm_type(subject.type) << ' ' << subject.name << ", ptr " << subject_pointer << '\n';
    output << "  " << result_pointer << " = alloca " << llvm_type(inferred->second) << '\n';

    if (subject.type.kind == ValueType::enum_type) {
        bool last_case_wildcard = false;
        for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
            const Expression& pattern = *expression.arguments[index];
            const Expression& result = *expression.arguments[index + 1];
            const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";
            last_case_wildcard = wildcard;
            const std::string next_label_name = next_label("case_next");
            const std::string case_label = next_label("case_arm");

            const TypeChecker::VariantResolution* resolution = nullptr;
            bool had_previous_binding = false;
            Local previous_binding;
            if (!wildcard) {
                resolution = &resolved_variants_.at(&pattern);
                const std::string subject_value = next_register();
                const std::string tag = next_register();
                const std::string matched = next_register();
                output << "  " << subject_value << " = load ptr, ptr " << subject_pointer << '\n';
                output << "  " << tag << " = load i64, ptr " << subject_value << '\n';
                output << "  " << matched << " = icmp eq i64 " << tag << ", " << resolution->tag << '\n';
                output << "  br i1 " << matched << ", label %" << case_label << ", label %" << next_label_name << '\n';
                output << case_label << ":\n";

                if (resolution->binds_payload) {
                    const auto previous = locals_.find(resolution->binding_name);
                    if (previous != locals_.end()) {
                        had_previous_binding = true;
                        previous_binding = previous->second;
                    }

                    const std::size_t payload_size = llvm_size(resolution->payload_type);
                    const std::size_t payload_offset = align_to(std::size_t{8}, payload_size);
                    const std::string payload_slot = next_register();
                    const std::string payload = next_register();
                    const std::string binding_pointer = next_register();
                    output << "  " << payload_slot << " = getelementptr i8, ptr " << subject_value << ", i64 "
                           << payload_offset << '\n';
                    output << "  " << payload << " = load " << llvm_type(resolution->payload_type) << ", ptr "
                           << payload_slot << '\n';
                    output << "  " << binding_pointer << " = alloca " << llvm_type(resolution->payload_type) << '\n';
                    output << "  store " << llvm_type(resolution->payload_type) << ' ' << payload << ", ptr "
                           << binding_pointer << '\n';
                    locals_[resolution->binding_name] = Local{binding_pointer, resolution->payload_type};
                }
            }

            const TypedValue value = emit_expression(result, output);
            if (resolution != nullptr && resolution->binds_payload) {
                if (had_previous_binding) {
                    locals_[resolution->binding_name] = previous_binding;
                } else {
                    locals_.erase(resolution->binding_name);
                }
            }

            output << "  store " << llvm_type(inferred->second) << ' ' << value.name << ", ptr " << result_pointer
                   << '\n';
            output << "  br label %" << end_label << '\n';
            if (!wildcard) {
                output << next_label_name << ":\n";
            }
        }

        if (!last_case_wildcard) {
            output << "  unreachable\n";
        }
        output << end_label << ":\n";
        const std::string value = next_register();
        output << "  " << value << " = load " << llvm_type(inferred->second) << ", ptr " << result_pointer << '\n';
        return TypedValue{value, inferred->second};
    }

    for (std::size_t index = 0; index < expression.arguments.size(); index += 2) {
        const Expression& pattern = *expression.arguments[index];
        const Expression& result = *expression.arguments[index + 1];
        const bool wildcard = pattern.kind == ExpressionKind::identifier && pattern.lexeme == "_";
        const std::string next_label_name = next_label("case_next");
        const std::string case_label = next_label("case_arm");

        if (!wildcard) {
            const std::string subject_value = next_register();
            const TypedValue pattern_value = emit_expression(pattern, output);
            const std::string matched = next_register();
            output << "  " << subject_value << " = load " << llvm_type(subject.type) << ", ptr " << subject_pointer
                   << '\n';
            if (subject.type.kind == ValueType::text_type) {
                const std::string comparison = next_register();
                output << "  " << comparison << " = call i32 @strcmp(ptr " << subject_value << ", ptr "
                       << pattern_value.name << ")\n";
                output << "  " << matched << " = icmp eq i32 " << comparison << ", 0\n";
            } else if (is_real_type(subject.type.kind)) {
                output << "  " << matched << " = fcmp oeq " << llvm_type(subject.type) << ' ' << subject_value << ", "
                       << pattern_value.name << '\n';
            } else {
                output << "  " << matched << " = icmp eq " << llvm_type(subject.type) << ' ' << subject_value << ", "
                       << pattern_value.name << '\n';
            }
            output << "  br i1 " << matched << ", label %" << case_label << ", label %" << next_label_name << '\n';
            output << case_label << ":\n";
        }

        const TypedValue value = emit_expression(result, output);
        output << "  store " << llvm_type(inferred->second) << ' ' << value.name << ", ptr " << result_pointer << '\n';
        output << "  br label %" << end_label << '\n';
        if (!wildcard) {
            output << next_label_name << ":\n";
        }
    }

    output << end_label << ":\n";
    const std::string value = next_register();
    output << "  " << value << " = load " << llvm_type(inferred->second) << ", ptr " << result_pointer << '\n';
    return TypedValue{value, inferred->second};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_unary_expression(const Expression& expression, std::ostream& output) {
    const TypedValue right = emit_expression(*expression.right, output);
    if (expression.lexeme == "!") {
        const std::string result = next_register();
        output << "  " << result << " = xor i1 " << right.name << ", 1\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    if (expression.lexeme == "-") {
        const std::string result = next_register();
        if (is_real_type(right.type.kind)) {
            output << "  " << result << " = fsub " << llvm_type(right.type) << ' ' << default_value(right.type) << ", "
                   << right.name << '\n';
        } else {
            output << "  " << result << " = sub " << llvm_type(right.type) << " 0, " << right.name << '\n';
        }

        return TypedValue{result, right.type};
    }

    throw std::runtime_error("unknown unary operator");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_cast_expression(const Expression& expression, std::ostream& output) {
    return cast_value(emit_expression(*expression.left, output), expression.type.type, output);
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_call_expression(const Expression& expression, std::ostream& output) {
    const std::string key = resolved_calls_.at(&expression);
    const auto function = functions_.find(key);
    if (function == functions_.end()) {
        throw std::runtime_error("undefined function '" + expression.lexeme + "'");
    }

    std::vector<TypedValue> arguments;
    arguments.reserve(expression.arguments.size());
    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        arguments.push_back(emit_expression(*argument, output));
    }

    const std::string result = next_register();
    output << "  " << result << " = call " << llvm_type(function->second.return_type) << ' '
           << (function->second.is_extern ? extern_function_name(function->second) : function_name(key)) << '(';
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }

        output << llvm_type(arguments[index].type) << ' ' << arguments[index].name;
    }
    output << ")\n";
    return TypedValue{result, function->second.return_type};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_method_call_expression(const Expression& expression,
                                                                         std::ostream& output) {
    const auto resolved = resolved_calls_.find(&expression);
    const auto function = resolved == resolved_calls_.end() ? functions_.end() : functions_.find(resolved->second);
    if (function != functions_.end()) {
        std::vector<TypedValue> arguments;
        arguments.reserve(function->second.parameters.size());
        if (function->second.parameters.size() == expression.arguments.size() + 1) {
            arguments.push_back(emit_expression(*expression.left, output));
        } else if (function->second.parameters.size() != expression.arguments.size()) {
            throw std::runtime_error("method call argument count mismatch");
        }

        for (const std::unique_ptr<Expression>& argument : expression.arguments) {
            arguments.push_back(emit_expression(*argument, output));
        }

        const std::string result = next_register();
        output << "  " << result << " = call " << llvm_type(function->second.return_type) << ' '
               << (function->second.is_extern ? extern_function_name(function->second)
                                              : function_name(resolved->second))
               << '(';
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }

            output << llvm_type(arguments[index].type) << ' ' << arguments[index].name;
        }
        output << ")\n";
        return TypedValue{result, function->second.return_type};
    }

    const auto receiver = expression_types_.find(expression.left.get());
    if (receiver == expression_types_.end()) {
        throw std::runtime_error("missing receiver type");
    }

    if (receiver->second.kind == ValueType::array_type) {
        return emit_array_method_call_expression(expression, output);
    }

    if (receiver->second.kind == ValueType::text_type) {
        return emit_text_method_call_expression(expression, output);
    }

    throw std::runtime_error("unknown method '" + expression.lexeme + "'");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_array_method_call_expression(const Expression& expression,
                                                                               std::ostream& output) {
    if (expression.lexeme == "len") {
        const TypedValue array = emit_expression(*expression.left, output);
        const std::string length = next_register();
        output << "  " << length << " = load i64, ptr " << array.name << '\n';
        return TypedValue{length, make_type(ValueType::int_type)};
    }

    if (expression.lexeme == "push") {
        const TypedValue array = emit_expression(*expression.left, output);
        const TypedValue value = emit_expression(*expression.arguments.at(0), output);
        if (array.type.element == nullptr) {
            throw std::runtime_error("array method 'push' called with non-array type");
        }

        const Type& element_type = *array.type.element;
        const std::size_t element_size = llvm_size(element_type);
        const std::string length = next_register();
        const std::string capacity_pointer = next_register();
        const std::string capacity = next_register();
        const std::string data_pointer_pointer = next_register();
        const std::string full = next_register();
        const std::string grow_label = next_label("array_grow");
        const std::string store_label = next_label("array_store");

        output << "  " << length << " = load i64, ptr " << array.name << '\n';
        output << "  " << capacity_pointer << " = getelementptr i8, ptr " << array.name << ", i64 8\n";
        output << "  " << capacity << " = load i64, ptr " << capacity_pointer << '\n';
        output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << array.name << ", i64 16\n";
        output << "  " << full << " = icmp eq i64 " << length << ", " << capacity << '\n';
        output << "  br i1 " << full << ", label %" << grow_label << ", label %" << store_label << '\n';
        output << grow_label << ":\n";

        const std::string zero_capacity = next_register();
        const std::string doubled_capacity = next_register();
        const std::string next_capacity = next_register();
        const std::string next_bytes = next_register();
        const std::string old_data = next_register();
        const std::string next_data = next_register();
        output << "  " << zero_capacity << " = icmp eq i64 " << capacity << ", 0\n";
        output << "  " << doubled_capacity << " = mul i64 " << capacity << ", 2\n";
        output << "  " << next_capacity << " = select i1 " << zero_capacity << ", i64 1, i64 " << doubled_capacity
               << '\n';
        output << "  " << next_bytes << " = mul i64 " << next_capacity << ", " << element_size << '\n';
        output << "  " << old_data << " = load ptr, ptr " << data_pointer_pointer << '\n';
        output << "  " << next_data << " = call ptr @dune_realloc(ptr " << old_data << ", i64 " << next_bytes << ")\n";
        output << "  store ptr " << next_data << ", ptr " << data_pointer_pointer << '\n';
        output << "  store i64 " << next_capacity << ", ptr " << capacity_pointer << '\n';
        output << "  br label %" << store_label << '\n';
        output << store_label << ":\n";

        const std::string data = next_register();
        const std::string offset = next_register();
        const std::string slot = next_register();
        const std::string next_length = next_register();
        output << "  " << data << " = load ptr, ptr " << data_pointer_pointer << '\n';
        output << "  " << offset << " = mul i64 " << length << ", " << element_size << '\n';
        output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << offset << '\n';
        output << "  store " << llvm_type(element_type) << ' ' << value.name << ", ptr " << slot << '\n';
        output << "  " << next_length << " = add i64 " << length << ", 1\n";
        output << "  store i64 " << next_length << ", ptr " << array.name << '\n';
        return TypedValue{"0", make_type(ValueType::unit_type)};
    }

    if (expression.lexeme == "pop") {
        const TypedValue array = emit_expression(*expression.left, output);
        if (array.type.element == nullptr) {
            throw std::runtime_error("array method 'pop' called with non-array type");
        }

        const Type& element_type = *array.type.element;
        const std::size_t element_size = llvm_size(element_type);
        const std::string length = next_register();
        const std::string empty = next_register();
        const std::string ok_label = next_label("array_pop_ok");
        const std::string panic_label = next_label("array_pop_panic");
        const std::string next_length = next_register();
        const std::string data_pointer_pointer = next_register();
        const std::string data = next_register();
        const std::string offset = next_register();
        const std::string slot = next_register();
        const std::string value = next_register();
        output << "  " << length << " = load i64, ptr " << array.name << '\n';
        output << "  " << empty << " = icmp eq i64 " << length << ", 0\n";
        output << "  br i1 " << empty << ", label %" << panic_label << ", label %" << ok_label << '\n';
        output << panic_label << ":\n";
        output << "  call i32 (ptr, ...) @printf(ptr @.dune_panic_array_pop)\n";
        output << "  call void @dune_free_all()\n";
        output << "  call void @exit(i32 1)\n";
        output << "  unreachable\n";
        output << ok_label << ":\n";
        output << "  " << next_length << " = sub i64 " << length << ", 1\n";
        output << "  store i64 " << next_length << ", ptr " << array.name << '\n';
        output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << array.name << ", i64 16\n";
        output << "  " << data << " = load ptr, ptr " << data_pointer_pointer << '\n';
        output << "  " << offset << " = mul i64 " << next_length << ", " << element_size << '\n';
        output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << offset << '\n';
        output << "  " << value << " = load " << llvm_type(element_type) << ", ptr " << slot << '\n';
        return TypedValue{value, element_type};
    }

    if (expression.lexeme == "clear") {
        const TypedValue array = emit_expression(*expression.left, output);
        output << "  store i64 0, ptr " << array.name << '\n';
        return TypedValue{"0", make_type(ValueType::unit_type)};
    }

    if (expression.lexeme == "is_empty") {
        const TypedValue array = emit_expression(*expression.left, output);
        const std::string length = next_register();
        const std::string result = next_register();
        output << "  " << length << " = load i64, ptr " << array.name << '\n';
        output << "  " << result << " = icmp eq i64 " << length << ", 0\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    throw std::runtime_error("unknown method '" + expression.lexeme + "'");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_text_method_call_expression(const Expression& expression,
                                                                              std::ostream& output) {
    if (expression.lexeme == "len") {
        const TypedValue text = emit_expression(*expression.left, output);
        const std::string result = next_register();
        output << "  " << result << " = call i64 @strlen(ptr " << text.name << ")\n";
        return TypedValue{result, make_type(ValueType::int_type)};
    }

    if (expression.lexeme == "is_empty") {
        const TypedValue text = emit_expression(*expression.left, output);
        const std::string length = next_register();
        const std::string result = next_register();
        output << "  " << length << " = call i64 @strlen(ptr " << text.name << ")\n";
        output << "  " << result << " = icmp eq i64 " << length << ", 0\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    if (expression.lexeme == "contains") {
        const TypedValue text = emit_expression(*expression.left, output);
        const TypedValue needle = emit_expression(*expression.arguments.at(0), output);
        const std::string found = next_register();
        const std::string result = next_register();
        output << "  " << found << " = call ptr @strstr(ptr " << text.name << ", ptr " << needle.name << ")\n";
        output << "  " << result << " = icmp ne ptr " << found << ", null\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    if (expression.lexeme == "starts_with") {
        const TypedValue text = emit_expression(*expression.left, output);
        const TypedValue prefix = emit_expression(*expression.arguments.at(0), output);
        const std::string prefix_length = next_register();
        const std::string comparison = next_register();
        const std::string result = next_register();
        output << "  " << prefix_length << " = call i64 @strlen(ptr " << prefix.name << ")\n";
        output << "  " << comparison << " = call i32 @strncmp(ptr " << text.name << ", ptr " << prefix.name << ", i64 "
               << prefix_length << ")\n";
        output << "  " << result << " = icmp eq i32 " << comparison << ", 0\n";
        return TypedValue{result, make_type(ValueType::bool_type)};
    }

    throw std::runtime_error("unknown text method '" + expression.lexeme + "'");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_array_literal(const Expression& expression, std::ostream& output) {
    const auto type = expression_types_.find(&expression);
    if (type == expression_types_.end() || type->second.element == nullptr) {
        throw std::runtime_error("missing inferred array type");
    }

    const Type& element_type = *type->second.element;
    const std::size_t element_size = llvm_size(element_type);
    const std::size_t length = expression.arguments.size();
    const std::size_t capacity = length == 0 ? 1 : length;

    const std::string handle = next_register();
    const std::string capacity_pointer = next_register();
    const std::string data_pointer_pointer = next_register();
    const std::string data = next_register();
    output << "  " << handle << " = call ptr @dune_alloc(i64 24)\n";
    output << "  store i64 " << length << ", ptr " << handle << '\n';
    output << "  " << capacity_pointer << " = getelementptr i8, ptr " << handle << ", i64 8\n";
    output << "  store i64 " << capacity << ", ptr " << capacity_pointer << '\n';
    output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << handle << ", i64 16\n";
    output << "  " << data << " = call ptr @dune_alloc(i64 " << capacity * element_size << ")\n";
    output << "  store ptr " << data << ", ptr " << data_pointer_pointer << '\n';

    for (std::size_t index = 0; index < expression.arguments.size(); ++index) {
        const TypedValue element = emit_expression(*expression.arguments[index], output);
        const std::string slot = next_register();
        output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << index * element_size << '\n';
        output << "  store " << llvm_type(element_type) << ' ' << element.name << ", ptr " << slot << '\n';
    }

    return TypedValue{handle, type->second};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_struct_literal(const Expression& expression, std::ostream& output) {
    const auto type = expression_types_.find(&expression);
    if (type == expression_types_.end() || type->second.kind != ValueType::struct_type) {
        throw std::runtime_error("missing inferred record literal type");
    }

    const StructLayout layout = concrete_struct_layout(type->second);
    const std::string handle = next_register();
    output << "  " << handle << " = call ptr @dune_alloc(i64 " << layout.size << ")\n";

    for (const Parameter& field : layout.fields) {
        const auto source = std::find(expression.field_names.begin(), expression.field_names.end(), field.name);
        if (source == expression.field_names.end()) {
            throw std::runtime_error("missing field '" + field.name + "'");
        }

        const std::size_t source_index = static_cast<std::size_t>(source - expression.field_names.begin());
        const TypedValue value = emit_expression(*expression.arguments.at(source_index), output);
        const std::size_t offset = layout.field_offsets.at(field.name);
        const std::string slot = next_register();
        output << "  " << slot << " = getelementptr i8, ptr " << handle << ", i64 " << offset << '\n';
        output << "  store " << llvm_type(field.type.type) << ' ' << value.name << ", ptr " << slot << '\n';
    }

    return TypedValue{handle, type->second};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_variant_constructor(const Expression& expression,
                                                                      std::ostream& output) {
    const auto type = expression_types_.find(&expression);
    if (type == expression_types_.end() || type->second.kind != ValueType::enum_type) {
        throw std::runtime_error("missing inferred choice variant type");
    }

    const TypeChecker::VariantResolution& resolution = resolved_variants_.at(&expression);
    const std::size_t payload_size = resolution.has_payload ? llvm_size(resolution.payload_type) : 0;
    const std::size_t payload_offset = resolution.has_payload ? align_to(std::size_t{8}, payload_size) : 8;
    const std::size_t allocation_size = resolution.has_payload ? payload_offset + payload_size : 8;
    const std::string handle = next_register();
    output << "  " << handle << " = call ptr @dune_alloc(i64 " << allocation_size << ")\n";
    output << "  store i64 " << resolution.tag << ", ptr " << handle << '\n';

    if (resolution.has_payload) {
        TypedValue payload;
        if (expression.kind == ExpressionKind::call || expression.kind == ExpressionKind::method_call) {
            payload = emit_expression(*expression.arguments.at(0), output);
        } else {
            throw std::runtime_error("choice variant payload constructor needs an argument");
        }

        const std::string slot = next_register();
        output << "  " << slot << " = getelementptr i8, ptr " << handle << ", i64 " << payload_offset << '\n';
        output << "  store " << llvm_type(resolution.payload_type) << ' ' << payload.name << ", ptr " << slot << '\n';
    }

    return TypedValue{handle, type->second};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_member_expression(const Expression& expression,
                                                                    std::ostream& output) {
    const auto receiver_type = expression_types_.find(expression.left.get());
    if (receiver_type != expression_types_.end() && receiver_type->second.kind == ValueType::struct_type) {
        const StructLayout layout = concrete_struct_layout(receiver_type->second);

        const auto index = layout.field_indices.find(expression.lexeme);
        if (index == layout.field_indices.end()) {
            throw std::runtime_error("unknown field '" + expression.lexeme + "'");
        }

        const Parameter& field = layout.fields[index->second];
        const TypedValue receiver = emit_expression(*expression.left, output);
        const std::string slot = next_register();
        const std::string value = next_register();
        output << "  " << slot << " = getelementptr i8, ptr " << receiver.name << ", i64 "
               << layout.field_offsets.at(expression.lexeme) << '\n';
        output << "  " << value << " = load " << llvm_type(field.type.type) << ", ptr " << slot << '\n';
        return TypedValue{value, field.type.type};
    }

    if (expression.left->kind == ExpressionKind::identifier) {
        const std::string name = expression.left->lexeme + "." + expression.lexeme;
        const auto local = locals_.find(name);
        if (local == locals_.end()) {
            throw std::runtime_error("undefined variable '" + name + "'");
        }

        const std::string value = next_register();
        output << "  " << value << " = load " << llvm_type(local->second.type) << ", ptr " << local->second.pointer
               << '\n';
        return TypedValue{value, local->second.type};
    }

    throw std::runtime_error("unknown member expression");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_index_expression(const Expression& expression, std::ostream& output) {
    const TypedValue indexed = emit_expression(*expression.left, output);
    const TypedValue index = emit_expression(*expression.right, output);
    const std::string index_name = emit_index_as_i64(index, output);

    if (indexed.type.kind == ValueType::text_type) {
        const std::string length = next_register();
        const std::string slot = next_register();
        const std::string value = next_register();
        output << "  " << length << " = call i64 @strlen(ptr " << indexed.name << ")\n";
        emit_bounds_check(index_name, length, "text", output);
        output << "  " << slot << " = getelementptr i8, ptr " << indexed.name << ", i64 " << index_name << '\n';
        output << "  " << value << " = load i8, ptr " << slot << '\n';
        return TypedValue{value, make_type(ValueType::glyph_type)};
    }

    if (indexed.type.element == nullptr) {
        throw std::runtime_error("index expression used with non-array type");
    }

    const Type& element_type = *indexed.type.element;
    const std::size_t element_size = llvm_size(element_type);

    const std::string length = next_register();
    const std::string data_pointer_pointer = next_register();
    const std::string data = next_register();
    const std::string offset = next_register();
    const std::string slot = next_register();
    const std::string value = next_register();
    output << "  " << length << " = load i64, ptr " << indexed.name << '\n';
    emit_bounds_check(index_name, length, "array", output);
    output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << indexed.name << ", i64 16\n";
    output << "  " << data << " = load ptr, ptr " << data_pointer_pointer << '\n';
    output << "  " << offset << " = mul i64 " << index_name << ", " << element_size << '\n';
    output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << offset << '\n';
    output << "  " << value << " = load " << llvm_type(element_type) << ", ptr " << slot << '\n';
    return TypedValue{value, element_type};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_slice_expression(const Expression& expression, std::ostream& output) {
    const TypedValue sliced = emit_expression(*expression.left, output);
    std::string start = "0";
    if (!expression.arguments.empty() && expression.arguments[0] != nullptr) {
        start = emit_index_as_i64(emit_expression(*expression.arguments[0], output), output);
    }

    std::string end;
    if (expression.arguments.size() > 1 && expression.arguments[1] != nullptr) {
        end = emit_index_as_i64(emit_expression(*expression.arguments[1], output), output);
    }

    const std::string length = next_register();
    if (sliced.type.kind == ValueType::text_type) {
        output << "  " << length << " = call i64 @strlen(ptr " << sliced.name << ")\n";
        if (end.empty()) {
            end = length;
        }
        emit_slice_bounds_check(start, end, length, output);

        const std::string slice_length = next_register();
        const std::string bytes = next_register();
        const std::string source = next_register();
        const std::string result = next_register();
        const std::string terminator = next_register();
        output << "  " << slice_length << " = sub i64 " << end << ", " << start << '\n';
        output << "  " << bytes << " = add i64 " << slice_length << ", 1\n";
        output << "  " << result << " = call ptr @dune_alloc(i64 " << bytes << ")\n";
        output << "  " << source << " = getelementptr i8, ptr " << sliced.name << ", i64 " << start << '\n';
        output << "  call ptr @memcpy(ptr " << result << ", ptr " << source << ", i64 " << slice_length << ")\n";
        output << "  " << terminator << " = getelementptr i8, ptr " << result << ", i64 " << slice_length << '\n';
        output << "  store i8 0, ptr " << terminator << '\n';
        return TypedValue{result, make_type(ValueType::text_type)};
    }

    if (sliced.type.kind == ValueType::array_type && sliced.type.element != nullptr) {
        output << "  " << length << " = load i64, ptr " << sliced.name << '\n';
        if (end.empty()) {
            end = length;
        }
        emit_slice_bounds_check(start, end, length, output);

        const Type& element_type = *sliced.type.element;
        const std::size_t element_size = llvm_size(element_type);
        const std::string slice_length = next_register();
        const std::string byte_length = next_register();
        const std::string handle = next_register();
        const std::string capacity_pointer = next_register();
        const std::string data_pointer_pointer = next_register();
        const std::string source_data_pointer_pointer = next_register();
        const std::string source_data = next_register();
        const std::string source_offset = next_register();
        const std::string source = next_register();
        const std::string data = next_register();
        output << "  " << slice_length << " = sub i64 " << end << ", " << start << '\n';
        output << "  " << byte_length << " = mul i64 " << slice_length << ", " << element_size << '\n';
        output << "  " << handle << " = call ptr @dune_alloc(i64 24)\n";
        output << "  store i64 " << slice_length << ", ptr " << handle << '\n';
        output << "  " << capacity_pointer << " = getelementptr i8, ptr " << handle << ", i64 8\n";
        output << "  store i64 " << slice_length << ", ptr " << capacity_pointer << '\n';
        output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << handle << ", i64 16\n";
        output << "  " << data << " = call ptr @dune_alloc(i64 " << byte_length << ")\n";
        output << "  store ptr " << data << ", ptr " << data_pointer_pointer << '\n';
        output << "  " << source_data_pointer_pointer << " = getelementptr i8, ptr " << sliced.name << ", i64 16\n";
        output << "  " << source_data << " = load ptr, ptr " << source_data_pointer_pointer << '\n';
        output << "  " << source_offset << " = mul i64 " << start << ", " << element_size << '\n';
        output << "  " << source << " = getelementptr i8, ptr " << source_data << ", i64 " << source_offset << '\n';
        output << "  call ptr @memcpy(ptr " << data << ", ptr " << source << ", i64 " << byte_length << ")\n";
        return TypedValue{handle, sliced.type};
    }

    throw std::runtime_error("slice expression used with non-array and non-text type");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_text_literal(const std::string& lexeme) {
    const std::string value = decode_text_literal(lexeme);
    const std::string name = "@.dune_text_" + std::to_string(string_literal_count_++);
    const std::size_t size = value.size() + 1;

    std::ostringstream global;
    global << name << " = private unnamed_addr constant [" << size << " x i8] c\"" << llvm_text_literal(value)
           << "\\00\"\n";
    string_globals_.push_back(global.str());

    return TypedValue{"getelementptr inbounds ([" + std::to_string(size) + " x i8], ptr " + name + ", i64 0, i64 0)",
                      make_type(ValueType::text_type)};
}

void LlvmIrGenerator::emit_print(const TypedValue& value, std::ostream& output) {
    const TypedValue printable = cast_for_print(value, output);
    const std::string format = next_register();
    output << "  " << format << " = getelementptr inbounds " << printf_format_name(value.type) << '\n';
    output << "  call i32 (ptr, ...) @printf(ptr " << format << ", " << llvm_type(printable.type) << ' '
           << printable.name << ")\n";
}

void LlvmIrGenerator::emit_bounds_check(const std::string& index, const std::string& length, std::string_view message,
                                        std::ostream& output) {
    const std::string negative = next_register();
    const std::string too_high = next_register();
    const std::string invalid = next_register();
    const std::string ok_label = next_label("bounds_ok");
    const std::string panic_label = next_label("bounds_panic");
    const std::string global = message == "text" ? "@.dune_panic_text_index" : "@.dune_panic_array_index";

    output << "  " << negative << " = icmp slt i64 " << index << ", 0\n";
    output << "  " << too_high << " = icmp uge i64 " << index << ", " << length << '\n';
    output << "  " << invalid << " = or i1 " << negative << ", " << too_high << '\n';
    output << "  br i1 " << invalid << ", label %" << panic_label << ", label %" << ok_label << '\n';
    output << panic_label << ":\n";
    output << "  call i32 (ptr, ...) @printf(ptr " << global << ")\n";
    output << "  call void @dune_free_all()\n";
    output << "  call void @exit(i32 1)\n";
    output << "  unreachable\n";
    output << ok_label << ":\n";
}

void LlvmIrGenerator::emit_slice_bounds_check(const std::string& start, const std::string& end,
                                              const std::string& length, std::ostream& output) {
    const std::string start_negative = next_register();
    const std::string end_negative = next_register();
    const std::string start_after_end = next_register();
    const std::string end_after_length = next_register();
    const std::string any_negative = next_register();
    const std::string bad_order_or_end = next_register();
    const std::string invalid = next_register();
    const std::string ok_label = next_label("slice_ok");
    const std::string panic_label = next_label("slice_panic");

    output << "  " << start_negative << " = icmp slt i64 " << start << ", 0\n";
    output << "  " << end_negative << " = icmp slt i64 " << end << ", 0\n";
    output << "  " << start_after_end << " = icmp ugt i64 " << start << ", " << end << '\n';
    output << "  " << end_after_length << " = icmp ugt i64 " << end << ", " << length << '\n';
    output << "  " << any_negative << " = or i1 " << start_negative << ", " << end_negative << '\n';
    output << "  " << bad_order_or_end << " = or i1 " << start_after_end << ", " << end_after_length << '\n';
    output << "  " << invalid << " = or i1 " << any_negative << ", " << bad_order_or_end << '\n';
    output << "  br i1 " << invalid << ", label %" << panic_label << ", label %" << ok_label << '\n';
    output << panic_label << ":\n";
    output << "  call i32 (ptr, ...) @printf(ptr @.dune_panic_slice)\n";
    output << "  call void @dune_free_all()\n";
    output << "  call void @exit(i32 1)\n";
    output << "  unreachable\n";
    output << ok_label << ":\n";
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::cast_for_print(const TypedValue& value, std::ostream& output) {
    if (value.type.kind == ValueType::bool_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext i1 " << value.name << " to i64\n";
        return TypedValue{result, make_type(ValueType::int_type)};
    }

    if (value.type.kind == ValueType::u8_type || value.type.kind == ValueType::u16_type ||
        value.type.kind == ValueType::u32_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext " << llvm_type(value.type) << ' ' << value.name << " to i64\n";
        return TypedValue{result, make_type(ValueType::u64_type)};
    }

    if (value.type.kind == ValueType::i8_type || value.type.kind == ValueType::i16_type ||
        value.type.kind == ValueType::i32_type) {
        const std::string result = next_register();
        output << "  " << result << " = sext " << llvm_type(value.type) << ' ' << value.name << " to i64\n";
        return TypedValue{result, make_type(ValueType::int_type)};
    }

    if (value.type.kind == ValueType::real32_type) {
        const std::string result = next_register();
        output << "  " << result << " = fpext float " << value.name << " to double\n";
        return TypedValue{result, make_type(ValueType::real_type)};
    }

    if (value.type.kind == ValueType::glyph_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext i8 " << value.name << " to i32\n";
        return TypedValue{result, make_type(ValueType::u32_type)};
    }

    return value;
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::cast_value(const TypedValue& value, const Type& target,
                                                        std::ostream& output) {
    if (value.type.kind == target.kind) {
        return TypedValue{value.name, target};
    }

    if (target.kind == ValueType::text_type || target.kind == ValueType::unit_type ||
        target.kind == ValueType::array_type || target.kind == ValueType::struct_type ||
        target.kind == ValueType::enum_type) {
        return TypedValue{value.name, target};
    }

    const std::string target_type = llvm_type(target);
    const std::string result = next_register();

    if (target.kind == ValueType::bool_type) {
        if (is_real_type(value.type.kind)) {
            output << "  " << result << " = fcmp one " << llvm_type(value.type) << ' ' << value.name << ", "
                   << default_value(value.type) << '\n';
        } else {
            output << "  " << result << " = icmp ne " << llvm_type(value.type) << ' ' << value.name << ", 0\n";
        }

        return TypedValue{result, target};
    }

    if (value.type.kind == ValueType::bool_type) {
        if (is_real_type(target.kind)) {
            output << "  " << result << " = uitofp i1 " << value.name << " to " << target_type << '\n';
        } else {
            output << "  " << result << " = zext i1 " << value.name << " to " << target_type << '\n';
        }

        return TypedValue{result, target};
    }

    if (is_real_type(value.type.kind) && is_real_type(target.kind)) {
        output << "  " << result << " = " << (llvm_bit_width(value.type) < llvm_bit_width(target) ? "fpext" : "fptrunc")
               << ' ' << llvm_type(value.type) << ' ' << value.name << " to " << target_type << '\n';
        return TypedValue{result, target};
    }

    if (is_real_type(value.type.kind) && (is_integer_type(target.kind) || target.kind == ValueType::glyph_type)) {
        output << "  " << result << " = "
               << (is_unsigned_type(target.kind) || target.kind == ValueType::glyph_type ? "fptoui" : "fptosi") << ' '
               << llvm_type(value.type) << ' ' << value.name << " to " << target_type << '\n';
        return TypedValue{result, target};
    }

    if ((is_integer_type(value.type.kind) || value.type.kind == ValueType::glyph_type) && is_real_type(target.kind)) {
        output << "  " << result << " = "
               << (is_unsigned_type(value.type.kind) || value.type.kind == ValueType::glyph_type ? "uitofp" : "sitofp")
               << ' ' << llvm_type(value.type) << ' ' << value.name << " to " << target_type << '\n';
        return TypedValue{result, target};
    }

    if (is_integer_type(value.type.kind) || value.type.kind == ValueType::glyph_type) {
        const std::size_t source_bits = llvm_bit_width(value.type);
        const std::size_t target_bits = llvm_bit_width(target);
        if (source_bits == target_bits) {
            return TypedValue{value.name, target};
        }

        if (source_bits < target_bits) {
            output << "  " << result << " = "
                   << (is_unsigned_type(value.type.kind) || value.type.kind == ValueType::glyph_type ? "zext" : "sext")
                   << ' ' << llvm_type(value.type) << ' ' << value.name << " to " << target_type << '\n';
        } else {
            output << "  " << result << " = trunc " << llvm_type(value.type) << ' ' << value.name << " to "
                   << target_type << '\n';
        }

        return TypedValue{result, target};
    }

    throw std::runtime_error("unsupported cast");
}

std::string LlvmIrGenerator::emit_index_as_i64(const TypedValue& index, std::ostream& output) {
    if (index.type.kind == ValueType::u8_type || index.type.kind == ValueType::u16_type ||
        index.type.kind == ValueType::u32_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext " << llvm_type(index.type) << ' ' << index.name << " to i64\n";
        return result;
    }

    if (index.type.kind == ValueType::i8_type || index.type.kind == ValueType::i16_type ||
        index.type.kind == ValueType::i32_type) {
        const std::string result = next_register();
        output << "  " << result << " = sext " << llvm_type(index.type) << ' ' << index.name << " to i64\n";
        return result;
    }

    return index.name;
}

std::string LlvmIrGenerator::next_register() {
    return "%r" + std::to_string(register_count_++);
}

std::string LlvmIrGenerator::next_label(std::string_view name) {
    return "dune_" + std::string(name) + "_" + std::to_string(label_count_++);
}

std::string LlvmIrGenerator::llvm_type(const Type& type) const {
    switch (type.kind) {
    case ValueType::int_type:
    case ValueType::i64_type:
    case ValueType::isize_type:
        return "i64";
    case ValueType::bool_type:
        return "i1";
    case ValueType::i8_type:
    case ValueType::u8_type:
    case ValueType::glyph_type:
        return "i8";
    case ValueType::i16_type:
    case ValueType::u16_type:
        return "i16";
    case ValueType::i32_type:
    case ValueType::u32_type:
        return "i32";
    case ValueType::u64_type:
    case ValueType::usize_type:
        return "i64";
    case ValueType::real32_type:
        return "float";
    case ValueType::real_type:
        return "double";
    case ValueType::text_type:
    case ValueType::array_type:
    case ValueType::struct_type:
    case ValueType::enum_type:
        return "ptr";
    case ValueType::unit_type:
        return "i8";
    case ValueType::generic_type:
        break;
    }

    throw std::runtime_error("unknown type");
}

std::size_t LlvmIrGenerator::llvm_bit_width(const Type& type) const {
    switch (type.kind) {
    case ValueType::bool_type:
        return 1;
    case ValueType::i8_type:
    case ValueType::u8_type:
    case ValueType::glyph_type:
        return 8;
    case ValueType::i16_type:
    case ValueType::u16_type:
        return 16;
    case ValueType::i32_type:
    case ValueType::u32_type:
    case ValueType::real32_type:
        return 32;
    case ValueType::int_type:
    case ValueType::i64_type:
    case ValueType::isize_type:
    case ValueType::u64_type:
    case ValueType::usize_type:
    case ValueType::real_type:
        return 64;
    case ValueType::text_type:
    case ValueType::unit_type:
    case ValueType::array_type:
    case ValueType::generic_type:
    case ValueType::struct_type:
    case ValueType::enum_type:
        break;
    }

    throw std::runtime_error("unknown scalar type");
}

std::string LlvmIrGenerator::printf_format_name(const Type& type) const {
    switch (type.kind) {
    case ValueType::int_type:
    case ValueType::i8_type:
    case ValueType::i16_type:
    case ValueType::i32_type:
    case ValueType::i64_type:
    case ValueType::isize_type:
    case ValueType::bool_type:
        return "[6 x i8], ptr @.dune_fmt_sint, i64 0, i64 0";
    case ValueType::u16_type:
    case ValueType::u8_type:
    case ValueType::u32_type:
    case ValueType::u64_type:
    case ValueType::usize_type:
        return "[6 x i8], ptr @.dune_fmt_uint, i64 0, i64 0";
    case ValueType::real32_type:
    case ValueType::real_type:
        return "[7 x i8], ptr @.dune_fmt_real, i64 0, i64 0";
    case ValueType::glyph_type:
        return "[4 x i8], ptr @.dune_fmt_glyph, i64 0, i64 0";
    case ValueType::text_type:
        return "[4 x i8], ptr @.dune_fmt_text, i64 0, i64 0";
    case ValueType::unit_type:
    case ValueType::array_type:
    case ValueType::generic_type:
    case ValueType::struct_type:
    case ValueType::enum_type:
        break;
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::function_name(const std::string& name) const {
    return "@dune_func_" + llvm_symbol(name);
}

std::string LlvmIrGenerator::extern_function_name(const FunctionSignature& signature) const {
    if (signature.extern_symbol.empty()) {
        throw std::runtime_error("missing foreign symbol");
    }

    return "@" + signature.extern_symbol;
}

Type LlvmIrGenerator::normalize_type(const Type& type) const {
    if (type.kind == ValueType::array_type) {
        Type result{ValueType::array_type, nullptr};
        if (type.element != nullptr) {
            result.element = std::make_shared<Type>(normalize_type(*type.element));
        }
        return result;
    }

    std::vector<Type> arguments;
    arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        arguments.push_back(normalize_type(argument));
    }

    if (type.kind == ValueType::generic_type && structs_.contains(type.name)) {
        return make_struct_type(type.name, std::move(arguments));
    }

    if (type.kind == ValueType::generic_type && enums_.contains(type.name)) {
        return make_enum_type(type.name, std::move(arguments));
    }

    Type result = type;
    result.arguments = std::move(arguments);
    return result;
}

Type LlvmIrGenerator::substitute_struct_type(const Type& type,
                                             const std::unordered_map<std::string, Type>& substitutions) const {
    if (type.kind == ValueType::generic_type) {
        const auto replacement = substitutions.find(type.name);
        if (replacement != substitutions.end()) {
            return replacement->second;
        }
    }

    if (type.kind == ValueType::array_type) {
        Type result{ValueType::array_type, nullptr};
        if (type.element != nullptr) {
            result.element = std::make_shared<Type>(substitute_struct_type(*type.element, substitutions));
        }
        return result;
    }

    Type result = type;
    result.arguments.clear();
    result.arguments.reserve(type.arguments.size());
    for (const Type& argument : type.arguments) {
        result.arguments.push_back(substitute_struct_type(argument, substitutions));
    }

    return result;
}

LlvmIrGenerator::StructLayout LlvmIrGenerator::concrete_struct_layout(const Type& type) const {
    const auto base = structs_.find(type.name);
    if (base == structs_.end()) {
        throw std::runtime_error("unknown record '" + type.name + "'");
    }

    if (base->second.generic_parameters.size() != type.arguments.size()) {
        throw std::runtime_error("record '" + type.name + "' expects " +
                                 std::to_string(base->second.generic_parameters.size()) + " type arguments but got " +
                                 std::to_string(type.arguments.size()));
    }

    std::unordered_map<std::string, Type> substitutions;
    for (std::size_t index = 0; index < base->second.generic_parameters.size(); ++index) {
        substitutions.emplace(base->second.generic_parameters[index].name, type.arguments[index]);
    }

    StructLayout layout;
    layout.generic_parameters = base->second.generic_parameters;
    for (const Parameter& field : base->second.fields) {
        const Type concrete_type = substitute_struct_type(field.type.type, substitutions);
        const std::size_t field_size = llvm_size(concrete_type);
        layout.size = align_to(layout.size, field_size);
        layout.field_indices.emplace(field.name, layout.fields.size());
        layout.field_offsets.emplace(field.name, layout.size);
        layout.fields.push_back(Parameter{field.name, TypeAnnotation{true, concrete_type}, field.location});
        layout.size += field_size;
    }

    if (layout.size == 0) {
        layout.size = 1;
    }

    return layout;
}

std::string LlvmIrGenerator::llvm_symbol(const std::string& value) const {
    std::string symbol;
    for (const unsigned char character : value) {
        if (std::isalnum(character) || character == '_') {
            symbol += static_cast<char>(character);
            continue;
        }

        std::ostringstream escaped;
        escaped << '_' << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(character);
        symbol += escaped.str();
    }

    return symbol;
}

std::size_t LlvmIrGenerator::llvm_size(const Type& type) const {
    switch (type.kind) {
    case ValueType::bool_type:
    case ValueType::i8_type:
    case ValueType::u8_type:
    case ValueType::glyph_type:
    case ValueType::unit_type:
        return 1;
    case ValueType::i16_type:
    case ValueType::u16_type:
        return 2;
    case ValueType::i32_type:
    case ValueType::u32_type:
    case ValueType::real32_type:
        return 4;
    case ValueType::int_type:
    case ValueType::i64_type:
    case ValueType::isize_type:
    case ValueType::u64_type:
    case ValueType::usize_type:
    case ValueType::real_type:
    case ValueType::text_type:
    case ValueType::array_type:
    case ValueType::struct_type:
    case ValueType::enum_type:
        return 8;
    case ValueType::generic_type:
        break;
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::default_value(const Type& type) const {
    if (is_real_type(type.kind)) {
        return "0.000000e+00";
    }

    if (type.kind == ValueType::text_type || type.kind == ValueType::array_type ||
        type.kind == ValueType::struct_type || type.kind == ValueType::enum_type) {
        return "null";
    }

    return "0";
}

std::string LlvmIrGenerator::decode_glyph_literal(const std::string& lexeme) const {
    char value = '\0';
    if (lexeme.size() == 3) {
        value = lexeme[1];
    } else if (lexeme.size() == 4 && lexeme[1] == '\\') {
        switch (lexeme[2]) {
        case 'n':
            value = '\n';
            break;
        case 'r':
            value = '\r';
            break;
        case 't':
            value = '\t';
            break;
        case '0':
            value = '\0';
            break;
        case '\'':
            value = '\'';
            break;
        case '\\':
            value = '\\';
            break;
        default:
            throw std::runtime_error("unknown glyph escape");
        }
    } else {
        throw std::runtime_error("invalid glyph literal");
    }

    return std::to_string(static_cast<unsigned char>(value));
}

std::string LlvmIrGenerator::decode_text_literal(const std::string& lexeme) const {
    std::string result;
    for (std::size_t index = 1; index + 1 < lexeme.size(); ++index) {
        char current = lexeme[index];
        if (current != '\\') {
            result += current;
            continue;
        }

        ++index;
        if (index + 1 >= lexeme.size()) {
            throw std::runtime_error("invalid text literal");
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
            throw std::runtime_error("unknown text escape");
        }
    }

    return result;
}

std::string LlvmIrGenerator::llvm_text_literal(const std::string& value) const {
    std::ostringstream output;
    for (const unsigned char character : value) {
        if (std::isprint(character) && character != '"' && character != '\\') {
            output << static_cast<char>(character);
            continue;
        }

        output << '\\' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(character)
               << std::nouppercase << std::dec;
    }

    return output.str();
}

} // namespace dune
