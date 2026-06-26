#pragma once

#include "compiler/bytecode.hpp"

#include <iosfwd>
#include <vector>

namespace dune {

class VirtualMachine {
public:
    explicit VirtualMachine(Bytecode bytecode);

    void run(std::ostream& output);

private:
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
    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
};

} // namespace dune
