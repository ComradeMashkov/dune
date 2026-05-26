#include "vm.hpp"

#include <iomanip>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace dune {

namespace {

Value make_signed(std::int64_t value) {
    Value result;
    result.kind = ValueKind::signed_integer;
    result.signed_value = value;
    return result;
}

Value make_unsigned(std::uint64_t value) {
    Value result;
    result.kind = ValueKind::unsigned_integer;
    result.unsigned_value = value;
    return result;
}

Value make_real(double value) {
    Value result;
    result.kind = ValueKind::real;
    result.real_value = value;
    return result;
}

Value make_bool(bool value) {
    Value result;
    result.kind = ValueKind::boolean;
    result.bool_value = value;
    return result;
}

Value make_unit() {
    Value result;
    result.kind = ValueKind::unit;
    return result;
}

Value make_array(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::array;
    result.array_value = std::make_shared<std::vector<Value>>(std::move(values));
    return result;
}

void expect_same_kind(const Value& left, const Value& right) {
    if (left.kind != right.kind) {
        throw std::runtime_error("runtime type mismatch");
    }
}

Value add_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value + right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value + right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value + right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
        break;
    }

    throw std::runtime_error("invalid addition operands");
}

Value subtract_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value - right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value - right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value - right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
        break;
    }

    throw std::runtime_error("invalid subtraction operands");
}

Value multiply_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return make_signed(left.signed_value * right.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(left.unsigned_value * right.unsigned_value);
    case ValueKind::real:
        return make_real(left.real_value * right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
        break;
    }

    throw std::runtime_error("invalid multiplication operands");
}

Value divide_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        if (right.signed_value == 0) {
            throw std::runtime_error("division by zero");
        }

        return make_signed(left.signed_value / right.signed_value);
    case ValueKind::unsigned_integer:
        if (right.unsigned_value == 0) {
            throw std::runtime_error("division by zero");
        }

        return make_unsigned(left.unsigned_value / right.unsigned_value);
    case ValueKind::real:
        if (right.real_value == 0.0) {
            throw std::runtime_error("division by zero");
        }

        return make_real(left.real_value / right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
        break;
    }

    throw std::runtime_error("invalid division operands");
}

bool values_equal(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return left.signed_value == right.signed_value;
    case ValueKind::unsigned_integer:
        return left.unsigned_value == right.unsigned_value;
    case ValueKind::real:
        return left.real_value == right.real_value;
    case ValueKind::boolean:
        return left.bool_value == right.bool_value;
    case ValueKind::glyph:
        return left.glyph_value == right.glyph_value;
    case ValueKind::text:
        return left.text_value == right.text_value;
    case ValueKind::unit:
        return true;
    case ValueKind::array:
        break;
    }

    return false;
}

int compare_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        return (left.signed_value > right.signed_value) - (left.signed_value < right.signed_value);
    case ValueKind::unsigned_integer:
        return (left.unsigned_value > right.unsigned_value) - (left.unsigned_value < right.unsigned_value);
    case ValueKind::real:
        return (left.real_value > right.real_value) - (left.real_value < right.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
        break;
    }

    throw std::runtime_error("invalid comparison operands");
}

bool is_false(const Value& value) {
    if (value.kind != ValueKind::boolean) {
        throw std::runtime_error("condition must be bool");
    }

    return !value.bool_value;
}

void print_value(const Value& value, std::ostream& output) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        output << value.signed_value << '\n';
        return;
    case ValueKind::unsigned_integer:
        output << value.unsigned_value << '\n';
        return;
    case ValueKind::real:
        output << std::setprecision(15) << value.real_value << '\n';
        return;
    case ValueKind::boolean:
        output << (value.bool_value ? 1 : 0) << '\n';
        return;
    case ValueKind::glyph:
        output << value.glyph_value << '\n';
        return;
    case ValueKind::text:
        output << value.text_value << '\n';
        return;
    case ValueKind::unit:
        throw std::runtime_error("cannot print unit value");
    case ValueKind::array:
        throw std::runtime_error("cannot print array value");
    }
}

std::size_t index_value(const Value& value) {
    if (value.kind == ValueKind::signed_integer) {
        if (value.signed_value < 0) {
            throw std::runtime_error("array index out of bounds");
        }

        return static_cast<std::size_t>(value.signed_value);
    }

    if (value.kind == ValueKind::unsigned_integer) {
        return static_cast<std::size_t>(value.unsigned_value);
    }

    throw std::runtime_error("array index must be integer");
}

std::vector<Value>& array_elements(const Value& value) {
    if (value.kind != ValueKind::array || value.array_value == nullptr) {
        throw std::runtime_error("expected array value");
    }

    return *value.array_value;
}

} // namespace

VirtualMachine::VirtualMachine(Bytecode bytecode) : bytecode_(std::move(bytecode)) {}

void VirtualMachine::run(std::ostream& output) {
    stack_.clear();
    frames_.clear();
    frames_.push_back(CallFrame{&bytecode_.instructions, 0, std::vector<Value>(bytecode_.local_count)});

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
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(add_values(left, right));
            ++frame.ip;
            break;
        }
        case OpCode::subtract: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(subtract_values(left, right));
            ++frame.ip;
            break;
        }
        case OpCode::multiply: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(multiply_values(left, right));
            ++frame.ip;
            break;
        }
        case OpCode::divide: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(divide_values(left, right));
            ++frame.ip;
            break;
        }
        case OpCode::equal: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(values_equal(left, right)));
            ++frame.ip;
            break;
        }
        case OpCode::not_equal: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(!values_equal(left, right)));
            ++frame.ip;
            break;
        }
        case OpCode::greater: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(compare_values(left, right) > 0));
            ++frame.ip;
            break;
        }
        case OpCode::greater_equal: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(compare_values(left, right) >= 0));
            ++frame.ip;
            break;
        }
        case OpCode::less: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(compare_values(left, right) < 0));
            ++frame.ip;
            break;
        }
        case OpCode::less_equal: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(make_bool(compare_values(left, right) <= 0));
            ++frame.ip;
            break;
        }
        case OpCode::jump_if_false:
            if (is_false(pop())) {
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
            const Value result = pop();
            frames_.pop_back();
            if (frames_.empty()) {
                return;
            }

            stack_.push_back(result);
            break;
        }
        case OpCode::pop:
            pop();
            ++frame.ip;
            break;
        case OpCode::make_array: {
            if (stack_.size() < instruction.operand) {
                throw std::runtime_error("not enough values on stack for array literal");
            }

            std::vector<Value> elements(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                elements[index - 1] = pop();
            }

            stack_.push_back(make_array(std::move(elements)));
            ++frame.ip;
            break;
        }
        case OpCode::load_index: {
            const Value index = pop();
            const Value array = pop();
            std::vector<Value>& elements = array_elements(array);
            const std::size_t offset = index_value(index);
            if (offset >= elements.size()) {
                throw std::runtime_error("array index out of bounds");
            }

            stack_.push_back(elements[offset]);
            ++frame.ip;
            break;
        }
        case OpCode::array_len: {
            const Value array = pop();
            stack_.push_back(make_signed(static_cast<std::int64_t>(array_elements(array).size())));
            ++frame.ip;
            break;
        }
        case OpCode::array_push: {
            const Value value = pop();
            const Value array = pop();
            array_elements(array).push_back(value);
            stack_.push_back(make_unit());
            ++frame.ip;
            break;
        }
        case OpCode::print:
            print_value(pop(), output);
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

    std::vector<Value> locals(function.local_count);
    for (std::size_t index = function.arity; index > 0; --index) {
        locals[index - 1] = pop();
    }

    frames_.push_back(CallFrame{&function.instructions, 0, std::move(locals)});
}

Value VirtualMachine::pop() {
    if (stack_.empty()) {
        throw std::runtime_error("stack underflow");
    }

    const Value value = stack_.back();
    stack_.pop_back();
    return value;
}

} // namespace dune
