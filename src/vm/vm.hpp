#pragma once

#include "compiler/bytecode.hpp"
#include "vm/runtime_error.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace dune {

class VirtualMachine {
public:
    explicit VirtualMachine(Bytecode bytecode);
    VirtualMachine(Bytecode bytecode, std::vector<std::string> program_arguments);

    void run(std::ostream& output);
    void run(std::ostream& output, std::ostream& error, std::istream& input);
    void run_test(std::size_t function_index, std::ostream& output);

private:
    void execute(std::ostream& output, std::ostream& error, std::istream& input);
    void execute_until(std::ostream& output, std::ostream& error, std::istream& input,
                       std::size_t frame_depth);

    struct CallFrame {
        const std::vector<Instruction>* instructions = nullptr;
        std::size_t ip = 0;
        std::vector<Value> locals;
        std::size_t stack_base = 0;
        std::vector<Value> deferred_calls;
        std::vector<std::size_t> defer_scope_starts;
        std::optional<Value> pending_return;
        bool awaiting_defer_result = false;
        std::string function_name = "<top-level>";
        SourceLocation declaration;
        std::size_t active_ip = 0;
        bool has_active_instruction = false;
    };

    void call_callable(const Value& callable);
    void call_function(std::size_t function_index, const std::vector<Value>& captures = {});
    void unwind_frames_to(std::size_t frame_depth, std::ostream& output, std::ostream& error,
                          std::istream& input, std::vector<RuntimeFailure>& cleanup_errors);
    std::vector<RuntimeStackFrame> capture_stack_trace() const;
    RuntimeError make_runtime_error(const std::exception& exception) const;
    Value call_extern_function(const Bytecode::Function& function, std::vector<Value> arguments);
    Value pop();

    Bytecode bytecode_;
    std::vector<std::string> program_arguments_;
    int log_level_ = 2;
    std::string plot_backend_ = "none";
    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
};

} // namespace dune
