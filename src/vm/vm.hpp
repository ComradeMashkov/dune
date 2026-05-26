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
    int pop();

    Bytecode bytecode_;
    std::vector<int> stack_;
    std::vector<int> locals_;
};

} // namespace dune
