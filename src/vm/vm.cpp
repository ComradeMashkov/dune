#include "vm.hpp"

#include <ostream>
#include <stdexcept>
#include <utility>

namespace dune {

VirtualMachine::VirtualMachine(Bytecode bytecode) : bytecode_(std::move(bytecode)) {}

void VirtualMachine::run(std::ostream& output) {
    stack_.clear();
    frames_.clear();
    frames_.push_back(CallFrame{&bytecode_.instructions, 0, std::vector<int>(bytecode_.local_count, 0)});

    while (!frames_.empty()) {
        CallFrame& frame = frames_.back();
        if (frame.ip >= frame.instructions->size()) {
            return;
        }

        const Instruction& instruction = frame.instructions->at(frame.ip);

        switch (instruction.op) {
        case OpCode::push_constant:
            stack_.push_back(bytecode_.constants.at(instruction.operand));
            ++frame.ip;
            break;
        case OpCode::load_local:
            stack_.push_back(frame.locals.at(instruction.operand));
            ++frame.ip;
            break;
        case OpCode::store_local:
            frame.locals.at(instruction.operand) = pop();
            ++frame.ip;
            break;
        case OpCode::add: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left + right);
            ++frame.ip;
            break;
        }
        case OpCode::subtract: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left - right);
            ++frame.ip;
            break;
        }
        case OpCode::multiply: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left * right);
            ++frame.ip;
            break;
        }
        case OpCode::divide: {
            const int right = pop();
            const int left = pop();
            if (right == 0) {
                throw std::runtime_error("division by zero");
            }

            stack_.push_back(left / right);
            ++frame.ip;
            break;
        }
        case OpCode::equal: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left == right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::not_equal: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left != right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::greater: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left > right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::greater_equal: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left >= right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::less: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left < right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::less_equal: {
            const int right = pop();
            const int left = pop();
            stack_.push_back(left <= right ? 1 : 0);
            ++frame.ip;
            break;
        }
        case OpCode::jump_if_false:
            if (pop() == 0) {
                frame.ip = instruction.operand;
            } else {
                ++frame.ip;
            }
            break;
        case OpCode::jump:
            frame.ip = instruction.operand;
            break;
        case OpCode::call:
            ++frame.ip;
            call_function(instruction.operand);
            break;
        case OpCode::return_value: {
            const int result = pop();
            frames_.pop_back();
            if (frames_.empty()) {
                return;
            }

            stack_.push_back(result);
            break;
        }
        case OpCode::print:
            output << pop() << '\n';
            ++frame.ip;
            break;
        case OpCode::halt:
            return;
        }
    }
}

void VirtualMachine::call_function(std::size_t function_index) {
    const Bytecode::Function& function = bytecode_.functions.at(function_index);
    if (stack_.size() < function.arity) {
        throw std::runtime_error("not enough arguments on stack for function call");
    }

    std::vector<int> locals(function.local_count, 0);
    for (std::size_t index = function.arity; index > 0; --index) {
        locals[index - 1] = pop();
    }

    frames_.push_back(CallFrame{&function.instructions, 0, std::move(locals)});
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
