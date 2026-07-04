#pragma once

#include "compiler/bytecode.hpp"

#include <iosfwd>
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

    struct CallFrame {
        const std::vector<Instruction>* instructions = nullptr;
        std::size_t ip = 0;
        std::vector<Value> locals;
        std::size_t stack_base = 0;
    };

    void call_function(std::size_t function_index);
    Value call_extern_function(const Bytecode::Function& function, std::vector<Value> arguments);
    Value pop();

    Bytecode bytecode_;
    std::vector<std::string> program_arguments_;
<<<<<<< HEAD
    int log_level_ = 2;
=======
    std::string plot_backend_ = "none";
>>>>>>> afa3caa (feat: add plot stdlib svg backend)
    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
};

} // namespace dune
