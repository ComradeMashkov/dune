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
    };

    void call_function(std::size_t function_index);
    Value pop();

    Bytecode bytecode_;
    std::vector<Value> stack_;
    std::vector<CallFrame> frames_;
};

} // namespace dune
