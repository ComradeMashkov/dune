#include "llvm_ir_generator.hpp"

#include "typechecker/type_checker.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

bool is_unsigned_type(ValueType type) {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type || type == ValueType::usize_type;
}

bool is_real_type(ValueType type) {
    return type == ValueType::real32_type || type == ValueType::real_type;
}

std::string real_literal(const std::string& lexeme) {
    std::ostringstream output;
    output << std::scientific << std::setprecision(16) << std::stod(lexeme);
    return output.str();
}

} // namespace

void LlvmIrGenerator::generate(const Program& program, std::ostream& output) {
    TypeChecker checker;
    checker.check(program);

    expression_types_ = checker.expression_types();
    resolved_calls_ = checker.resolved_calls();
    functions_.clear();
    string_globals_.clear();
    collect_functions(program);
    register_count_ = 0;
    label_count_ = 0;
    string_literal_count_ = 0;

    std::ostringstream body;

    locals_.clear();
    current_return_type_ = make_type(ValueType::int_type);
    body << "define i32 @main() {\n";
    emit_statements(program.statements, body);
    body << "  ret i32 0\n";
    body << "}\n\n";

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            emit_function(statement, body);
        }
    }

    output << "@.dune_fmt_sint = private unnamed_addr constant [6 x i8] c\"%lld\\0A\\00\"\n";
    output << "@.dune_fmt_uint = private unnamed_addr constant [6 x i8] c\"%llu\\0A\\00\"\n";
    output << "@.dune_fmt_real = private unnamed_addr constant [7 x i8] c\"%.15g\\0A\\00\"\n";
    output << "@.dune_fmt_glyph = private unnamed_addr constant [4 x i8] c\"%c\\0A\\00\"\n";
    output << "@.dune_fmt_text = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\"\n";
    for (const std::string& global : string_globals_) {
        output << global;
    }

    output << "declare i32 @printf(ptr, ...)\n";
    output << "declare i32 @strcmp(ptr, ptr)\n\n";
    output << "declare ptr @malloc(i64)\n";
    output << "declare ptr @realloc(ptr, i64)\n\n";
    output << body.str();
}

void LlvmIrGenerator::collect_functions(const Program& program) {
    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            collect_function(statement);
        }
    }
}

void LlvmIrGenerator::collect_function(const Statement& statement) {
    FunctionSignature signature;
    signature.return_type = statement.type.has_type ? statement.type.type : make_type(ValueType::int_type);

    for (const Parameter& parameter : statement.parameters) {
        signature.parameters.push_back(parameter.type.has_type ? parameter.type.type : make_type(ValueType::int_type));
    }

    functions_.emplace(function_key(statement.name, signature.parameters), std::move(signature));
}

void LlvmIrGenerator::emit_function(const Statement& statement, std::ostream& output) {
    std::vector<Type> parameters;
    parameters.reserve(statement.parameters.size());
    for (const Parameter& parameter : statement.parameters) {
        parameters.push_back(parameter.type.has_type ? parameter.type.type : make_type(ValueType::int_type));
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

    const bool terminated = emit_statements(statement.body, output);
    if (!terminated) {
        output << "  ret " << llvm_type(signature->second.return_type) << ' '
               << default_value(signature->second.return_type) << '\n';
    }

    output << "}\n\n";
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
    case StatementKind::let: {
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
        const bool body_terminated = emit_statements(statement.body, output);
        if (!body_terminated) {
            output << "  br label %" << condition_label << '\n';
        }

        output << end_label << ":\n";
        return false;
    }
    case StatementKind::function:
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
            return TypedValue{real_literal(expression.lexeme), type->second};
        }

        return TypedValue{expression.lexeme, type->second};
    case ExpressionKind::floating:
        return TypedValue{real_literal(expression.lexeme), type->second};
    case ExpressionKind::character:
        return TypedValue{decode_glyph_literal(expression.lexeme), make_type(ValueType::glyph_type)};
    case ExpressionKind::string:
        return emit_text_literal(expression.lexeme);
    case ExpressionKind::boolean:
        return TypedValue{expression.lexeme == "true" ? "1" : "0", make_type(ValueType::bool_type)};
    case ExpressionKind::array:
        return emit_array_literal(expression, output);
    case ExpressionKind::index:
        return emit_index_expression(expression, output);
    case ExpressionKind::binary:
        return emit_binary_expression(expression, output);
    case ExpressionKind::call:
        return emit_call_expression(expression, output);
    case ExpressionKind::method_call:
        return emit_method_call_expression(expression, output);
    }

    throw std::runtime_error("unknown expression");
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_binary_expression(const Expression& expression,
                                                                    std::ostream& output) {
    const TypedValue left = emit_expression(*expression.left, output);
    const TypedValue right = emit_expression(*expression.right, output);
    const std::string result = next_register();

    if (expression.lexeme == "+" || expression.lexeme == "-" || expression.lexeme == "*" || expression.lexeme == "/") {
        std::string op;
        if (is_real_type(left.type.kind)) {
            if (expression.lexeme == "+") {
                op = "fadd";
            } else if (expression.lexeme == "-") {
                op = "fsub";
            } else if (expression.lexeme == "*") {
                op = "fmul";
            } else {
                op = "fdiv";
            }
        } else if (expression.lexeme == "/") {
            op = is_unsigned_type(left.type.kind) ? "udiv" : "sdiv";
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
    output << "  " << result << " = call " << llvm_type(function->second.return_type) << ' ' << function_name(key)
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

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_method_call_expression(const Expression& expression,
                                                                         std::ostream& output) {
    if (expression.left->kind == ExpressionKind::identifier) {
        const auto resolved = resolved_calls_.find(&expression);
        const auto function = resolved == resolved_calls_.end() ? functions_.end() : functions_.find(resolved->second);
        if (function != functions_.end()) {
            std::vector<TypedValue> arguments;
            arguments.reserve(expression.arguments.size());
            for (const std::unique_ptr<Expression>& argument : expression.arguments) {
                arguments.push_back(emit_expression(*argument, output));
            }

            const std::string result = next_register();
            output << "  " << result << " = call " << llvm_type(function->second.return_type) << ' '
                   << function_name(resolved->second) << '(';
            for (std::size_t index = 0; index < arguments.size(); ++index) {
                if (index > 0) {
                    output << ", ";
                }

                output << llvm_type(arguments[index].type) << ' ' << arguments[index].name;
            }
            output << ")\n";
            return TypedValue{result, function->second.return_type};
        }
    }

    return emit_array_method_call_expression(expression, output);
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
        output << "  " << next_data << " = call ptr @realloc(ptr " << old_data << ", i64 " << next_bytes << ")\n";
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

    throw std::runtime_error("unknown method '" + expression.lexeme + "'");
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
    output << "  " << handle << " = call ptr @malloc(i64 24)\n";
    output << "  store i64 " << length << ", ptr " << handle << '\n';
    output << "  " << capacity_pointer << " = getelementptr i8, ptr " << handle << ", i64 8\n";
    output << "  store i64 " << capacity << ", ptr " << capacity_pointer << '\n';
    output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << handle << ", i64 16\n";
    output << "  " << data << " = call ptr @malloc(i64 " << capacity * element_size << ")\n";
    output << "  store ptr " << data << ", ptr " << data_pointer_pointer << '\n';

    for (std::size_t index = 0; index < expression.arguments.size(); ++index) {
        const TypedValue element = emit_expression(*expression.arguments[index], output);
        const std::string slot = next_register();
        output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << index * element_size << '\n';
        output << "  store " << llvm_type(element_type) << ' ' << element.name << ", ptr " << slot << '\n';
    }

    return TypedValue{handle, type->second};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_index_expression(const Expression& expression, std::ostream& output) {
    const TypedValue array = emit_expression(*expression.left, output);
    const TypedValue index = emit_expression(*expression.right, output);
    if (array.type.element == nullptr) {
        throw std::runtime_error("index expression used with non-array type");
    }

    const Type& element_type = *array.type.element;
    const std::size_t element_size = llvm_size(element_type);
    std::string index_name = index.name;
    if (index.type.kind == ValueType::u8_type || index.type.kind == ValueType::u16_type ||
        index.type.kind == ValueType::u32_type) {
        index_name = next_register();
        output << "  " << index_name << " = zext " << llvm_type(index.type) << ' ' << index.name << " to i64\n";
    } else if (index.type.kind == ValueType::i8_type || index.type.kind == ValueType::i16_type ||
               index.type.kind == ValueType::i32_type) {
        index_name = next_register();
        output << "  " << index_name << " = sext " << llvm_type(index.type) << ' ' << index.name << " to i64\n";
    }

    const std::string data_pointer_pointer = next_register();
    const std::string data = next_register();
    const std::string offset = next_register();
    const std::string slot = next_register();
    const std::string value = next_register();
    output << "  " << data_pointer_pointer << " = getelementptr i8, ptr " << array.name << ", i64 16\n";
    output << "  " << data << " = load ptr, ptr " << data_pointer_pointer << '\n';
    output << "  " << offset << " = mul i64 " << index_name << ", " << element_size << '\n';
    output << "  " << slot << " = getelementptr i8, ptr " << data << ", i64 " << offset << '\n';
    output << "  " << value << " = load " << llvm_type(element_type) << ", ptr " << slot << '\n';
    return TypedValue{value, element_type};
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
        return "ptr";
    case ValueType::unit_type:
        return "i8";
    }

    throw std::runtime_error("unknown type");
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
        break;
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::function_name(const std::string& name) const {
    return "@dune_fn_" + llvm_symbol(name);
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
        return 8;
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::default_value(const Type& type) const {
    if (is_real_type(type.kind)) {
        return "0.000000e+00";
    }

    if (type.kind == ValueType::text_type || type.kind == ValueType::array_type) {
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
