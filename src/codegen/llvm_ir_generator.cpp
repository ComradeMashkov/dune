#include "llvm_ir_generator.hpp"

#include "typechecker/type_checker.hpp"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

bool is_unsigned_type(ValueType type) {
    return type == ValueType::u8_type || type == ValueType::u16_type || type == ValueType::u32_type ||
           type == ValueType::u64_type;
}

bool is_real_type(ValueType type) {
    return type == ValueType::real_type;
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
    functions_.clear();
    collect_functions(program);
    register_count_ = 0;
    label_count_ = 0;

    output << "@.dune_fmt_sint = private unnamed_addr constant [6 x i8] c\"%lld\\0A\\00\"\n";
    output << "@.dune_fmt_uint = private unnamed_addr constant [6 x i8] c\"%llu\\0A\\00\"\n";
    output << "@.dune_fmt_real = private unnamed_addr constant [7 x i8] c\"%.15g\\0A\\00\"\n";
    output << "@.dune_fmt_glyph = private unnamed_addr constant [4 x i8] c\"%c\\0A\\00\"\n";
    output << "declare i32 @printf(ptr, ...)\n\n";

    locals_.clear();
    current_return_type_ = ValueType::int_type;
    output << "define i32 @main() {\n";
    emit_statements(program.statements, output);
    output << "  ret i32 0\n";
    output << "}\n\n";

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            emit_function(statement, output);
        }
    }
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
    signature.return_type = statement.type.has_type ? statement.type.type : ValueType::int_type;

    for (const Parameter& parameter : statement.parameters) {
        signature.parameters.push_back(parameter.type.has_type ? parameter.type.type : ValueType::int_type);
    }

    functions_.emplace(statement.name, std::move(signature));
}

void LlvmIrGenerator::emit_function(const Statement& statement, std::ostream& output) {
    const auto signature = functions_.find(statement.name);
    if (signature == functions_.end()) {
        throw std::runtime_error("undefined function '" + statement.name + "'");
    }

    locals_.clear();
    current_return_type_ = signature->second.return_type;

    output << "define " << llvm_type(signature->second.return_type) << " " << function_name(statement.name) << "(";
    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }

        output << llvm_type(signature->second.parameters[index]) << " %arg" << index;
    }
    output << ") {\n";

    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        const Parameter& parameter = statement.parameters[index];
        const ValueType type = signature->second.parameters[index];
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
        const TypedValue value = emit_expression(*statement.expression, output);
        output << "  ret " << llvm_type(current_return_type_) << ' ' << value.name << '\n';
        return true;
    }
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
        if (type->second == ValueType::real_type) {
            return TypedValue{real_literal(expression.lexeme), type->second};
        }

        return TypedValue{expression.lexeme, type->second};
    case ExpressionKind::floating:
        return TypedValue{real_literal(expression.lexeme), ValueType::real_type};
    case ExpressionKind::character:
        return TypedValue{decode_glyph_literal(expression.lexeme), ValueType::glyph_type};
    case ExpressionKind::boolean:
        return TypedValue{expression.lexeme == "true" ? "1" : "0", ValueType::bool_type};
    case ExpressionKind::binary:
        return emit_binary_expression(expression, output);
    case ExpressionKind::call:
        return emit_call_expression(expression, output);
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
        if (is_real_type(left.type)) {
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
            op = is_unsigned_type(left.type) ? "udiv" : "sdiv";
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

    std::string op;
    if (expression.lexeme == "==") {
        op = is_real_type(left.type) ? "oeq" : "eq";
    } else if (expression.lexeme == "!=") {
        op = is_real_type(left.type) ? "one" : "ne";
    } else if (expression.lexeme == ">") {
        op = is_real_type(left.type) ? "ogt" : (is_unsigned_type(left.type) ? "ugt" : "sgt");
    } else if (expression.lexeme == ">=") {
        op = is_real_type(left.type) ? "oge" : (is_unsigned_type(left.type) ? "uge" : "sge");
    } else if (expression.lexeme == "<") {
        op = is_real_type(left.type) ? "olt" : (is_unsigned_type(left.type) ? "ult" : "slt");
    } else if (expression.lexeme == "<=") {
        op = is_real_type(left.type) ? "ole" : (is_unsigned_type(left.type) ? "ule" : "sle");
    } else {
        throw std::runtime_error("unknown binary operator");
    }

    output << "  " << result << " = " << (is_real_type(left.type) ? "fcmp " : "icmp ") << op << ' '
           << llvm_type(left.type) << ' ' << left.name << ", " << right.name << '\n';
    return TypedValue{result, ValueType::bool_type};
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::emit_call_expression(const Expression& expression, std::ostream& output) {
    const auto function = functions_.find(expression.lexeme);
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
           << function_name(expression.lexeme) << '(';
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }

        output << llvm_type(arguments[index].type) << ' ' << arguments[index].name;
    }
    output << ")\n";
    return TypedValue{result, function->second.return_type};
}

void LlvmIrGenerator::emit_print(const TypedValue& value, std::ostream& output) {
    const TypedValue printable = cast_for_print(value, output);
    const std::string format = next_register();
    output << "  " << format << " = getelementptr inbounds " << printf_format_name(value.type) << '\n';
    output << "  call i32 (ptr, ...) @printf(ptr " << format << ", " << llvm_type(printable.type) << ' '
           << printable.name << ")\n";
}

LlvmIrGenerator::TypedValue LlvmIrGenerator::cast_for_print(const TypedValue& value, std::ostream& output) {
    if (value.type == ValueType::bool_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext i1 " << value.name << " to i64\n";
        return TypedValue{result, ValueType::int_type};
    }

    if (value.type == ValueType::u8_type || value.type == ValueType::u16_type || value.type == ValueType::u32_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext " << llvm_type(value.type) << ' ' << value.name << " to i64\n";
        return TypedValue{result, ValueType::u64_type};
    }

    if (value.type == ValueType::glyph_type) {
        const std::string result = next_register();
        output << "  " << result << " = zext i8 " << value.name << " to i32\n";
        return TypedValue{result, ValueType::u32_type};
    }

    return value;
}

std::string LlvmIrGenerator::next_register() {
    return "%r" + std::to_string(register_count_++);
}

std::string LlvmIrGenerator::next_label(std::string_view name) {
    return "dune_" + std::string(name) + "_" + std::to_string(label_count_++);
}

std::string LlvmIrGenerator::llvm_type(ValueType type) const {
    switch (type) {
    case ValueType::int_type:
        return "i64";
    case ValueType::bool_type:
        return "i1";
    case ValueType::u8_type:
        return "i8";
    case ValueType::u16_type:
        return "i16";
    case ValueType::u32_type:
        return "i32";
    case ValueType::u64_type:
        return "i64";
    case ValueType::real_type:
        return "double";
    case ValueType::glyph_type:
        return "i8";
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::printf_format_name(ValueType type) const {
    switch (type) {
    case ValueType::int_type:
    case ValueType::bool_type:
        return "[6 x i8], ptr @.dune_fmt_sint, i64 0, i64 0";
    case ValueType::u8_type:
    case ValueType::u16_type:
    case ValueType::u32_type:
    case ValueType::u64_type:
        return "[6 x i8], ptr @.dune_fmt_uint, i64 0, i64 0";
    case ValueType::real_type:
        return "[7 x i8], ptr @.dune_fmt_real, i64 0, i64 0";
    case ValueType::glyph_type:
        return "[4 x i8], ptr @.dune_fmt_glyph, i64 0, i64 0";
    }

    throw std::runtime_error("unknown type");
}

std::string LlvmIrGenerator::function_name(const std::string& name) const {
    return "@dune_fn_" + name;
}

std::string LlvmIrGenerator::default_value(ValueType type) const {
    if (type == ValueType::real_type) {
        return "0.000000e+00";
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

} // namespace dune
