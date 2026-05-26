#include "vm.hpp"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace dune {

VirtualMachine::VirtualMachine(Bytecode bytecode)
    : bytecode_(std::move(bytecode)), locals_(bytecode_.local_count, 0) {}

void VirtualMachine::run(std::ostream& output) {
    for (std::size_t ip = 0; ip < bytecode_.instructions.size(); ++ip) {
        const Instruction& instruction = bytecode_.instructions[ip];

        switch (instruction.op) {
        case OpCode::push_constant:
            stack_.push_back(bytecode_.constants.at(instruction.operand));
            break;
        case OpCode::load_local:
            stack_.push_back(locals_.at(instruction.operand));
            break;
        case OpCode::store_local:
            locals_.at(instruction.operand) = pop();
            break;
        case OpCode::add: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left + right);
            break;
        }
        case OpCode::subtract: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left - right);
            break;
        }
        case OpCode::multiply: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left * right);
            break;
        }
        case OpCode::divide: {
            const int right = pop();
            const int left = pop();
            if (right == 0) {
                throw std::runtime_error("division by zero");
            }

            stack_.push_back(left / right);
            break;
        }
        case OpCode::print:
            output << pop() << '\n';
            break;
        case OpCode::halt:
            return;
        }
    }
}

int VirtualMachine::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("stack underflow");
    }

    const int value = stack_.back();
    stack_.pop_back();
    return value;
}

} // namespace dune
