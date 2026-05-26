#include "assembly_generator.hpp"

#include <stdexcept>
#include <string_view>

namespace dune {

namespace {

std::size_t align_to(std::size_t value, std::size_t alignment) {
    const std::size_t remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }

    return value + alignment - remainder;
}

} // namespace

void AssemblyGenerator::generate(const Program& program, std::ostream& output) {
    assign_slots(program);
    label_count_ = 0;
    current_frame_ = &main_frame_;

    emit_header(output);
    emit_statements(program.statements, output);
    emit_footer(output);

    for (const Statement& statement : program.statements) {
        if (statement.kind == StatementKind::function) {
            emit_function(statement, output);
        }
    }

    current_frame_ = nullptr;
}

void AssemblyGenerator::assign_slots(const Program& program) {
    main_frame_ = SlotFrame{};
    function_frames_.clear();
    current_frame_ = &main_frame_;
    assign_slots(program.statements);

    main_frame_.stack_size = align_to(main_frame_.slots.size() * 8, 16);

    for (const Statement& statement : program.statements) {
        if (statement.kind != StatementKind::function) {
            continue;
        }

        SlotFrame frame;
        frame.return_label = function_label(statement.name) + "_return";
        current_frame_ = &frame;
        for (const Parameter& parameter : statement.parameters) {
            declare_slot(parameter.name);
            frame.declared.insert(parameter.name);
        }

        assign_slots(statement.body);
        frame.stack_size = align_to(frame.slots.size() * 8, 16);
        function_frames_.emplace(statement.name, std::move(frame));
    }

    current_frame_ = nullptr;
}

void AssemblyGenerator::assign_slots(const std::vector<Statement>& statements) {
    for (const Statement& statement : statements) {
        if (statement.kind == StatementKind::function) {
            continue;
        }

        if (statement.kind == StatementKind::let) {
            declare_slot(statement.name);
        }

        assign_slots(statement.body);
        assign_slots(statement.else_body);
    }
}

void AssemblyGenerator::emit_header(std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << ".section __TEXT,__cstring,cstring_literals\n";
    output << "L_dune_fmt:\n";
    output << "    .asciz \"%ld\\n\"\n";
    output << ".section __TEXT,__text,regular,pure_instructions\n";
    output << ".globl _main\n";
    output << ".p2align 2\n";
    output << "_main:\n";
    output << "    stp x29, x30, [sp, #-16]!\n";
    output << "    mov x29, sp\n";
    if (main_frame_.stack_size > 0) {
        output << "    sub sp, sp, #" << main_frame_.stack_size << '\n';
    }
#elif defined(__linux__) && defined(__x86_64__)
    output << ".intel_syntax noprefix\n";
    output << ".section .rodata\n";
    output << ".L_dune_fmt:\n";
    output << "    .string \"%ld\\n\"\n";
    output << ".text\n";
    output << ".globl main\n";
    output << ".extern printf\n";
    output << "main:\n";
    output << "    push rbp\n";
    output << "    mov rbp, rsp\n";
    if (main_frame_.stack_size > 0) {
        output << "    sub rsp, " << main_frame_.stack_size << '\n';
    }
#else
    (void)output;
    throw std::runtime_error("native assembly output is not supported on this platform");
#endif
}

void AssemblyGenerator::emit_footer(std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    mov w0, #0\n";
    output << "    mov sp, x29\n";
    output << "    ldp x29, x30, [sp], #16\n";
    output << "    ret\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov eax, 0\n";
    output << "    leave\n";
    output << "    ret\n";
#else
    (void)output;
#endif
}

void AssemblyGenerator::emit_function(const Statement& statement, std::ostream& output) {
    auto frame = function_frames_.find(statement.name);
    if (frame == function_frames_.end()) {
        throw std::runtime_error("undefined function '" + statement.name + "'");
    }

    current_frame_ = &frame->second;
    emit_function_header(statement, output);
    emit_statements(statement.body, output);
    emit_function_footer(output);
}

void AssemblyGenerator::emit_function_header(const Statement& statement, std::ostream& output) const {
    if (statement.parameters.size() > 8) {
        throw std::runtime_error("native assembly output supports at most 8 function parameters");
    }

    output << function_label(statement.name) << ":\n";
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    stp x29, x30, [sp, #-16]!\n";
    output << "    mov x29, sp\n";
    if (current_frame_->stack_size > 0) {
        output << "    sub sp, sp, #" << current_frame_->stack_size << '\n';
    }
#elif defined(__linux__) && defined(__x86_64__)
    if (statement.parameters.size() > 6) {
        throw std::runtime_error("native assembly output supports at most 6 parameters on x86_64");
    }

    output << "    push rbp\n";
    output << "    mov rbp, rsp\n";
    if (current_frame_->stack_size > 0) {
        output << "    sub rsp, " << current_frame_->stack_size << '\n';
    }
#else
    (void)output;
    throw std::runtime_error("native assembly output is not supported on this platform");
#endif

    for (std::size_t index = 0; index < statement.parameters.size(); ++index) {
        emit_store_argument(resolve_slot(statement.parameters[index].name), index, output);
    }
}

void AssemblyGenerator::emit_function_footer(std::ostream& output) const {
    emit_label(current_frame_->return_label, output);
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    mov sp, x29\n";
    output << "    ldp x29, x30, [sp], #16\n";
    output << "    ret\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    leave\n";
    output << "    ret\n";
#else
    (void)output;
#endif
}

void AssemblyGenerator::emit_statements(const std::vector<Statement>& statements, std::ostream& output) {
    for (const Statement& statement : statements) {
        emit_statement(statement, output);
    }
}

void AssemblyGenerator::emit_statement(const Statement& statement, std::ostream& output) {
    switch (statement.kind) {
    case StatementKind::let:
        emit_expression(*statement.expression, output);
        current_frame_->declared.insert(statement.name);
        emit_store(resolve_slot(statement.name), output);
        return;
    case StatementKind::assign:
        emit_expression(*statement.expression, output);
        emit_store(resolve_slot(statement.name), output);
        return;
    case StatementKind::print:
        emit_expression(*statement.expression, output);
        emit_print(output);
        return;
    case StatementKind::block:
        emit_statements(statement.body, output);
        return;
    case StatementKind::if_statement: {
        const std::string else_label = next_label("else");
        const std::string end_label = next_label("endif");
        emit_expression(*statement.expression, output);
        emit_branch_if_false(else_label, output);
        emit_statements(statement.body, output);

        if (statement.else_body.empty()) {
            emit_label(else_label, output);
            return;
        }

        emit_jump(end_label, output);
        emit_label(else_label, output);
        emit_statements(statement.else_body, output);
        emit_label(end_label, output);
        return;
    }
    case StatementKind::while_statement: {
        const std::string loop_label = next_label("while");
        const std::string end_label = next_label("endwhile");
        emit_label(loop_label, output);
        emit_expression(*statement.expression, output);
        emit_branch_if_false(end_label, output);
        emit_statements(statement.body, output);
        emit_jump(loop_label, output);
        emit_label(end_label, output);
        return;
    }
    case StatementKind::function:
        return;
    case StatementKind::return_statement:
        emit_expression(*statement.expression, output);
        emit_jump(current_frame_->return_label, output);
        return;
    }
}

void AssemblyGenerator::emit_expression(const Expression& expression, std::ostream& output) {
    switch (expression.kind) {
    case ExpressionKind::identifier:
        emit_load(resolve_slot(expression.lexeme), output);
        return;
    case ExpressionKind::number:
        emit_number(expression.lexeme, output);
        return;
    case ExpressionKind::boolean:
        emit_number(expression.lexeme == "true" ? "1" : "0", output);
        return;
    case ExpressionKind::binary:
        emit_expression(*expression.left, output);
#if defined(__APPLE__) && defined(__aarch64__)
        output << "    str x0, [sp, #-16]!\n";
#elif defined(__linux__) && defined(__x86_64__)
        output << "    sub rsp, 16\n";
        output << "    mov QWORD PTR [rsp], rax\n";
#else
        throw std::runtime_error("native assembly output is not supported on this platform");
#endif
        emit_expression(*expression.right, output);
        emit_binary(expression.lexeme, output);
        return;
    case ExpressionKind::call:
        emit_call(expression, output);
        return;
    }
}

void AssemblyGenerator::emit_branch_if_false(const std::string& label, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    cbz x0, " << label << '\n';
#elif defined(__linux__) && defined(__x86_64__)
    output << "    test rax, rax\n";
    output << "    jz " << label << '\n';
#else
    (void)label;
    (void)output;
#endif
}

void AssemblyGenerator::emit_jump(const std::string& label, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    b " << label << '\n';
#elif defined(__linux__) && defined(__x86_64__)
    output << "    jmp " << label << '\n';
#else
    (void)label;
    (void)output;
#endif
}

void AssemblyGenerator::emit_label(const std::string& label, std::ostream& output) const {
    output << label << ":\n";
}

void AssemblyGenerator::emit_print(std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    mov x1, x0\n";
    output << "    adrp x0, L_dune_fmt@PAGE\n";
    output << "    add x0, x0, L_dune_fmt@PAGEOFF\n";
    output << "    sub sp, sp, #16\n";
    output << "    str x1, [sp]\n";
    output << "    bl _printf\n";
    output << "    add sp, sp, #16\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov rsi, rax\n";
    output << "    lea rdi, [rip + .L_dune_fmt]\n";
    output << "    xor eax, eax\n";
    output << "    call printf@PLT\n";
#else
    (void)output;
#endif
}

void AssemblyGenerator::emit_store(std::size_t slot, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    str x0, [x29, #-" << slot_offset(slot) << "]\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov QWORD PTR [rbp - " << slot_offset(slot) << "], rax\n";
#else
    (void)slot;
    (void)output;
#endif
}

void AssemblyGenerator::emit_load(std::size_t slot, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    ldr x0, [x29, #-" << slot_offset(slot) << "]\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov rax, QWORD PTR [rbp - " << slot_offset(slot) << "]\n";
#else
    (void)slot;
    (void)output;
#endif
}

void AssemblyGenerator::emit_number(const std::string& number, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    mov x0, #" << number << '\n';
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov rax, " << number << '\n';
#else
    (void)number;
    (void)output;
#endif
}

void AssemblyGenerator::emit_binary(const std::string& op, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    ldr x1, [sp], #16\n";
    if (op == "+") {
        output << "    add x0, x1, x0\n";
        return;
    }

    if (op == "-") {
        output << "    sub x0, x1, x0\n";
        return;
    }

    if (op == "*") {
        output << "    mul x0, x1, x0\n";
        return;
    }

    if (op == "/") {
        output << "    sdiv x0, x1, x0\n";
        return;
    }

    output << "    cmp x1, x0\n";
    if (op == "==") {
        output << "    cset w0, eq\n";
        return;
    }

    if (op == "!=") {
        output << "    cset w0, ne\n";
        return;
    }

    if (op == ">") {
        output << "    cset w0, gt\n";
        return;
    }

    if (op == ">=") {
        output << "    cset w0, ge\n";
        return;
    }

    if (op == "<") {
        output << "    cset w0, lt\n";
        return;
    }

    if (op == "<=") {
        output << "    cset w0, le\n";
        return;
    }
#elif defined(__linux__) && defined(__x86_64__)
    output << "    mov rcx, QWORD PTR [rsp]\n";
    output << "    add rsp, 16\n";
    if (op == "+") {
        output << "    add rax, rcx\n";
        return;
    }

    if (op == "-") {
        output << "    sub rcx, rax\n";
        output << "    mov rax, rcx\n";
        return;
    }

    if (op == "*") {
        output << "    imul rax, rcx\n";
        return;
    }

    if (op == "/") {
        output << "    mov r8, rax\n";
        output << "    mov rax, rcx\n";
        output << "    cqo\n";
        output << "    idiv r8\n";
        return;
    }

    output << "    cmp rcx, rax\n";
    if (op == "==") {
        output << "    sete al\n";
        output << "    movzx rax, al\n";
        return;
    }

    if (op == "!=") {
        output << "    setne al\n";
        output << "    movzx rax, al\n";
        return;
    }

    if (op == ">") {
        output << "    setg al\n";
        output << "    movzx rax, al\n";
        return;
    }

    if (op == ">=") {
        output << "    setge al\n";
        output << "    movzx rax, al\n";
        return;
    }

    if (op == "<") {
        output << "    setl al\n";
        output << "    movzx rax, al\n";
        return;
    }

    if (op == "<=") {
        output << "    setle al\n";
        output << "    movzx rax, al\n";
        return;
    }
#else
    (void)output;
#endif

    throw std::runtime_error("unknown binary operator");
}

void AssemblyGenerator::emit_call(const Expression& expression, std::ostream& output) {
    for (const std::unique_ptr<Expression>& argument : expression.arguments) {
        emit_expression(*argument, output);
        emit_push_result(output);
    }

    for (std::size_t index = expression.arguments.size(); index > 0; --index) {
        emit_pop_argument(index - 1, expression.arguments.size(), output);
    }

#if defined(__APPLE__) && defined(__aarch64__)
    output << "    bl " << function_label(expression.lexeme) << '\n';
#elif defined(__linux__) && defined(__x86_64__)
    output << "    call " << function_label(expression.lexeme) << '\n';
#else
    (void)output;
    throw std::runtime_error("native assembly output is not supported on this platform");
#endif
}

void AssemblyGenerator::emit_push_result(std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    str x0, [sp, #-16]!\n";
#elif defined(__linux__) && defined(__x86_64__)
    output << "    sub rsp, 16\n";
    output << "    mov QWORD PTR [rsp], rax\n";
#else
    (void)output;
#endif
}

void AssemblyGenerator::emit_pop_argument(std::size_t argument_index, std::size_t argument_count,
                                          std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    if (argument_count > 8) {
        throw std::runtime_error("native assembly output supports at most 8 function arguments");
    }

    output << "    ldr x" << argument_index << ", [sp], #16\n";
#elif defined(__linux__) && defined(__x86_64__)
    if (argument_count > 6) {
        throw std::runtime_error("native assembly output supports at most 6 arguments on x86_64");
    }

    static constexpr const char* registers[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    output << "    mov " << registers[argument_index] << ", QWORD PTR [rsp]\n";
    output << "    add rsp, 16\n";
#else
    (void)argument_index;
    (void)argument_count;
    (void)output;
#endif
}

void AssemblyGenerator::emit_store_argument(std::size_t slot, std::size_t argument_index, std::ostream& output) const {
#if defined(__APPLE__) && defined(__aarch64__)
    output << "    str x" << argument_index << ", [x29, #-" << slot_offset(slot) << "]\n";
#elif defined(__linux__) && defined(__x86_64__)
    static constexpr const char* registers[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
    output << "    mov QWORD PTR [rbp - " << slot_offset(slot) << "], " << registers[argument_index] << '\n';
#else
    (void)slot;
    (void)argument_index;
    (void)output;
#endif
}

std::size_t AssemblyGenerator::declare_slot(const std::string& name) {
    const auto existing = current_frame_->slots.find(name);
    if (existing != current_frame_->slots.end()) {
        return existing->second;
    }

    const std::size_t slot = current_frame_->slots.size();
    current_frame_->slots.emplace(name, slot);
    return slot;
}

std::size_t AssemblyGenerator::resolve_slot(const std::string& name) const {
    if (!current_frame_->declared.contains(name)) {
        throw std::runtime_error("undefined variable '" + name + "'");
    }

    const auto existing = current_frame_->slots.find(name);
    if (existing == current_frame_->slots.end()) {
        throw std::runtime_error("undefined variable '" + name + "'");
    }

    return existing->second;
}

std::size_t AssemblyGenerator::slot_offset(std::size_t slot) const {
    return (slot + 1) * 8;
}

std::string AssemblyGenerator::function_label(const std::string& name) const {
    return ".L_dune_fn_" + name;
}

std::string AssemblyGenerator::next_label(std::string_view name) {
    return ".L_dune_" + std::string(name) + "_" + std::to_string(label_count_++);
}

} // namespace dune
