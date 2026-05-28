#include "vm.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <ostream>
#include <sstream>
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

Value make_glyph(char value) {
    Value result;
    result.kind = ValueKind::glyph;
    result.glyph_value = value;
    return result;
}

Value make_text(std::string value) {
    Value result;
    result.kind = ValueKind::text;
    result.text_value = std::move(value);
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

Value make_record(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::record;
    result.record_value = std::make_shared<std::vector<Value>>(std::move(values));
    return result;
}

Value make_variant(std::size_t tag, std::shared_ptr<Value> payload) {
    Value result;
    result.kind = ValueKind::variant;
    result.variant_tag = tag;
    result.variant_payload = std::move(payload);
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
    case ValueKind::record:
    case ValueKind::variant:
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
    case ValueKind::record:
    case ValueKind::variant:
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
    case ValueKind::record:
    case ValueKind::variant:
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
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid division operands");
}

Value modulo_values(const Value& left, const Value& right) {
    expect_same_kind(left, right);
    switch (left.kind) {
    case ValueKind::signed_integer:
        if (right.signed_value == 0) {
            throw std::runtime_error("division by zero");
        }

        return make_signed(left.signed_value % right.signed_value);
    case ValueKind::unsigned_integer:
        if (right.unsigned_value == 0) {
            throw std::runtime_error("division by zero");
        }

        return make_unsigned(left.unsigned_value % right.unsigned_value);
    case ValueKind::real:
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid modulo operands");
}

Value negate_value(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_signed(0 - value.signed_value);
    case ValueKind::unsigned_integer:
        return make_unsigned(0 - value.unsigned_value);
    case ValueKind::real:
        return make_real(0.0 - value.real_value);
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid unary minus operand");
}

Value not_value(const Value& value) {
    if (value.kind != ValueKind::boolean) {
        throw std::runtime_error("invalid logical not operand");
    }

    return make_bool(!value.bool_value);
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
    case ValueKind::record:
    case ValueKind::variant:
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
    case ValueKind::record:
    case ValueKind::variant:
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
    case ValueKind::record:
        throw std::runtime_error("cannot print record value");
    case ValueKind::variant:
        throw std::runtime_error("cannot print choice value");
    }

    throw std::runtime_error("cannot print unknown value");
}

std::string value_to_text(const Value& value) {
    std::ostringstream output;
    switch (value.kind) {
    case ValueKind::signed_integer:
        output << value.signed_value;
        return output.str();
    case ValueKind::unsigned_integer:
        output << value.unsigned_value;
        return output.str();
    case ValueKind::real:
        output << std::setprecision(15) << value.real_value;
        return output.str();
    case ValueKind::boolean:
        output << (value.bool_value ? 1 : 0);
        return output.str();
    case ValueKind::glyph:
        output << value.glyph_value;
        return output.str();
    case ValueKind::text:
        return value.text_value;
    case ValueKind::unit:
        throw std::runtime_error("cannot print unit value");
    case ValueKind::array:
        throw std::runtime_error("cannot print array value");
    case ValueKind::record:
        throw std::runtime_error("cannot print record value");
    case ValueKind::variant:
        throw std::runtime_error("cannot print choice value");
    }

    throw std::runtime_error("cannot print unknown value");
}

void print_formatted_value(const std::string& format, const std::vector<Value>& arguments, std::ostream& output) {
    std::size_t argument_index = 0;
    for (std::size_t index = 0; index < format.size(); ++index) {
        if (format[index] == '{' && index + 1 < format.size() && format[index + 1] == '}') {
            if (argument_index >= arguments.size()) {
                throw std::runtime_error("not enough print format arguments");
            }

            output << value_to_text(arguments[argument_index++]);
            ++index;
            continue;
        }

        output << format[index];
    }

    if (argument_index != arguments.size()) {
        throw std::runtime_error("too many print format arguments");
    }

    output << '\n';
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

std::vector<Value>& record_fields(const Value& value) {
    if (value.kind != ValueKind::record || value.record_value == nullptr) {
        throw std::runtime_error("expected record value");
    }

    return *value.record_value;
}

bool is_default_bound(const Value& value) {
    return value.kind == ValueKind::signed_integer && value.signed_value == -1;
}

std::size_t slice_bound(const Value& value, std::size_t default_value, std::size_t length) {
    if (is_default_bound(value)) {
        return default_value;
    }

    const std::size_t bound = index_value(value);
    if (bound > length) {
        throw std::runtime_error("slice bound out of bounds");
    }

    return bound;
}

double numeric_argument(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return static_cast<double>(value.signed_value);
    case ValueKind::unsigned_integer:
        return static_cast<double>(value.unsigned_value);
    case ValueKind::real:
        return value.real_value;
    case ValueKind::boolean:
    case ValueKind::glyph:
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("foreign function expected numeric argument");
}

Value cast_signed(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return value;
    case ValueKind::unsigned_integer:
        return make_signed(static_cast<std::int64_t>(value.unsigned_value));
    case ValueKind::real:
        return make_signed(static_cast<std::int64_t>(value.real_value));
    case ValueKind::boolean:
        return make_signed(value.bool_value ? 1 : 0);
    case ValueKind::glyph:
        return make_signed(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid signed cast operand");
}

Value cast_unsigned(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_unsigned(static_cast<std::uint64_t>(value.signed_value));
    case ValueKind::unsigned_integer:
        return value;
    case ValueKind::real:
        return make_unsigned(static_cast<std::uint64_t>(value.real_value));
    case ValueKind::boolean:
        return make_unsigned(value.bool_value ? 1 : 0);
    case ValueKind::glyph:
        return make_unsigned(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid unsigned cast operand");
}

Value cast_real(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_real(static_cast<double>(value.signed_value));
    case ValueKind::unsigned_integer:
        return make_real(static_cast<double>(value.unsigned_value));
    case ValueKind::real:
        return value;
    case ValueKind::boolean:
        return make_real(value.bool_value ? 1.0 : 0.0);
    case ValueKind::glyph:
        return make_real(static_cast<unsigned char>(value.glyph_value));
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid real cast operand");
}

Value cast_bool(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_bool(value.signed_value != 0);
    case ValueKind::unsigned_integer:
        return make_bool(value.unsigned_value != 0);
    case ValueKind::real:
        return make_bool(value.real_value != 0.0);
    case ValueKind::boolean:
        return value;
    case ValueKind::glyph:
        return make_bool(value.glyph_value != '\0');
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid bool cast operand");
}

Value cast_glyph(const Value& value) {
    switch (value.kind) {
    case ValueKind::signed_integer:
        return make_glyph(static_cast<char>(value.signed_value));
    case ValueKind::unsigned_integer:
        return make_glyph(static_cast<char>(value.unsigned_value));
    case ValueKind::real:
        return make_glyph(static_cast<char>(value.real_value));
    case ValueKind::boolean:
        return make_glyph(static_cast<char>(value.bool_value ? 1 : 0));
    case ValueKind::glyph:
        return value;
    case ValueKind::text:
    case ValueKind::unit:
    case ValueKind::array:
    case ValueKind::record:
    case ValueKind::variant:
        break;
    }

    throw std::runtime_error("invalid glyph cast operand");
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
        case OpCode::modulo: {
            const Value right = pop();
            const Value left = pop();
            stack_.push_back(modulo_values(left, right));
            ++frame.ip;
            break;
        }
        case OpCode::negate:
            stack_.push_back(negate_value(pop()));
            ++frame.ip;
            break;
        case OpCode::not_value:
            stack_.push_back(not_value(pop()));
            ++frame.ip;
            break;
        case OpCode::cast_signed:
            stack_.push_back(cast_signed(pop()));
            ++frame.ip;
            break;
        case OpCode::cast_unsigned:
            stack_.push_back(cast_unsigned(pop()));
            ++frame.ip;
            break;
        case OpCode::cast_real:
            stack_.push_back(cast_real(pop()));
            ++frame.ip;
            break;
        case OpCode::cast_bool:
            stack_.push_back(cast_bool(pop()));
            ++frame.ip;
            break;
        case OpCode::cast_glyph:
            stack_.push_back(cast_glyph(pop()));
            ++frame.ip;
            break;
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
        case OpCode::make_record: {
            if (stack_.size() < instruction.operand) {
                throw std::runtime_error("not enough values on stack for record literal");
            }

            std::vector<Value> fields(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                fields[index - 1] = pop();
            }

            stack_.push_back(make_record(std::move(fields)));
            ++frame.ip;
            break;
        }
        case OpCode::make_variant: {
            stack_.push_back(make_variant(instruction.operand, std::make_shared<Value>(pop())));
            ++frame.ip;
            break;
        }
        case OpCode::make_unit_variant:
            stack_.push_back(make_variant(instruction.operand, nullptr));
            ++frame.ip;
            break;
        case OpCode::load_variant_tag: {
            const Value value = pop();
            if (value.kind != ValueKind::variant) {
                throw std::runtime_error("expected choice value");
            }

            stack_.push_back(make_unsigned(value.variant_tag));
            ++frame.ip;
            break;
        }
        case OpCode::load_variant_payload: {
            const Value value = pop();
            if (value.kind != ValueKind::variant || value.variant_payload == nullptr) {
                throw std::runtime_error("expected choice variant payload");
            }

            stack_.push_back(*value.variant_payload);
            ++frame.ip;
            break;
        }
        case OpCode::load_index: {
            const Value index = pop();
            const Value indexed = pop();
            const std::size_t offset = index_value(index);

            if (indexed.kind == ValueKind::array) {
                std::vector<Value>& elements = array_elements(indexed);
                if (offset >= elements.size()) {
                    throw std::runtime_error("array index out of bounds");
                }

                stack_.push_back(elements[offset]);
                ++frame.ip;
                break;
            }

            if (indexed.kind == ValueKind::text) {
                if (offset >= indexed.text_value.size()) {
                    throw std::runtime_error("text index out of bounds");
                }

                stack_.push_back(make_glyph(indexed.text_value[offset]));
                ++frame.ip;
                break;
            }

            throw std::runtime_error("expected array or text value");
        }
        case OpCode::load_field: {
            const Value record = pop();
            std::vector<Value>& fields = record_fields(record);
            if (instruction.operand >= fields.size()) {
                throw std::runtime_error("record field out of bounds");
            }

            stack_.push_back(fields[instruction.operand]);
            ++frame.ip;
            break;
        }
        case OpCode::store_index: {
            const Value value = pop();
            const Value index = pop();
            const Value indexed = pop();
            const std::size_t offset = index_value(index);

            if (indexed.kind != ValueKind::array) {
                throw std::runtime_error("expected array value");
            }

            std::vector<Value>& elements = array_elements(indexed);
            if (offset >= elements.size()) {
                throw std::runtime_error("array index out of bounds");
            }

            elements[offset] = value;
            ++frame.ip;
            break;
        }
        case OpCode::store_field: {
            const Value value = pop();
            const Value record = pop();
            std::vector<Value>& fields = record_fields(record);
            if (instruction.operand >= fields.size()) {
                throw std::runtime_error("record field out of bounds");
            }

            fields[instruction.operand] = value;
            ++frame.ip;
            break;
        }
        case OpCode::load_slice: {
            const Value end_value = pop();
            const Value start_value = pop();
            const Value sliced = pop();

            if (sliced.kind == ValueKind::array) {
                std::vector<Value>& elements = array_elements(sliced);
                const std::size_t start = slice_bound(start_value, 0, elements.size());
                const std::size_t end = slice_bound(end_value, elements.size(), elements.size());
                if (start > end) {
                    throw std::runtime_error("slice start cannot be greater than slice end");
                }

                stack_.push_back(make_array(std::vector<Value>(elements.begin() + static_cast<std::ptrdiff_t>(start),
                                                               elements.begin() + static_cast<std::ptrdiff_t>(end))));
                ++frame.ip;
                break;
            }

            if (sliced.kind == ValueKind::text) {
                const std::size_t start = slice_bound(start_value, 0, sliced.text_value.size());
                const std::size_t end = slice_bound(end_value, sliced.text_value.size(), sliced.text_value.size());
                if (start > end) {
                    throw std::runtime_error("slice start cannot be greater than slice end");
                }

                stack_.push_back(make_text(sliced.text_value.substr(start, end - start)));
                ++frame.ip;
                break;
            }

            throw std::runtime_error("expected array or text value");
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
        case OpCode::array_pop: {
            const Value array = pop();
            std::vector<Value>& elements = array_elements(array);
            if (elements.empty()) {
                throw std::runtime_error("cannot pop from empty array");
            }

            stack_.push_back(elements.back());
            elements.pop_back();
            ++frame.ip;
            break;
        }
        case OpCode::array_clear: {
            const Value array = pop();
            array_elements(array).clear();
            stack_.push_back(make_unit());
            ++frame.ip;
            break;
        }
        case OpCode::array_is_empty: {
            const Value array = pop();
            stack_.push_back(make_bool(array_elements(array).empty()));
            ++frame.ip;
            break;
        }
        case OpCode::text_len: {
            const Value text = pop();
            if (text.kind != ValueKind::text) {
                throw std::runtime_error("expected text value");
            }

            stack_.push_back(make_signed(static_cast<std::int64_t>(text.text_value.size())));
            ++frame.ip;
            break;
        }
        case OpCode::text_is_empty: {
            const Value text = pop();
            if (text.kind != ValueKind::text) {
                throw std::runtime_error("expected text value");
            }

            stack_.push_back(make_bool(text.text_value.empty()));
            ++frame.ip;
            break;
        }
        case OpCode::text_contains: {
            const Value needle = pop();
            const Value text = pop();
            if (text.kind != ValueKind::text || needle.kind != ValueKind::text) {
                throw std::runtime_error("expected text value");
            }

            stack_.push_back(make_bool(text.text_value.find(needle.text_value) != std::string::npos));
            ++frame.ip;
            break;
        }
        case OpCode::text_starts_with: {
            const Value prefix = pop();
            const Value text = pop();
            if (text.kind != ValueKind::text || prefix.kind != ValueKind::text) {
                throw std::runtime_error("expected text value");
            }

            stack_.push_back(make_bool(text.text_value.starts_with(prefix.text_value)));
            ++frame.ip;
            break;
        }
        case OpCode::print:
            print_value(pop(), output);
            ++frame.ip;
            break;
        case OpCode::print_format: {
            std::vector<Value> arguments(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                arguments[index - 1] = pop();
            }

            const Value format = pop();
            if (format.kind != ValueKind::text) {
                throw std::runtime_error("print format string must be text");
            }

            print_formatted_value(format.text_value, arguments, output);
            ++frame.ip;
            break;
        }
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

    if (function.is_extern) {
        std::vector<Value> arguments(function.arity);
        for (std::size_t index = function.arity; index > 0; --index) {
            arguments[index - 1] = pop();
        }

        stack_.push_back(call_extern_function(function, std::move(arguments)));
        return;
    }

    std::vector<Value> locals(function.local_count);
    for (std::size_t index = function.arity; index > 0; --index) {
        locals[index - 1] = pop();
    }

    frames_.push_back(CallFrame{&function.instructions, 0, std::move(locals)});
}

Value VirtualMachine::call_extern_function(const Bytecode::Function& function, std::vector<Value> arguments) {
    const std::string& symbol = function.extern_symbol.empty() ? function.name : function.extern_symbol;
    if (symbol == "dune_panic" && arguments.size() == 1 && arguments[0].kind == ValueKind::text) {
        throw std::runtime_error(arguments[0].text_value);
    }

    if (arguments.size() == 1) {
        const double value = numeric_argument(arguments[0]);
        if (symbol == "sqrt" || symbol == "sqrtf") {
            return make_real(std::sqrt(value));
        }
        if (symbol == "sin" || symbol == "sinf") {
            return make_real(std::sin(value));
        }
        if (symbol == "cos" || symbol == "cosf") {
            return make_real(std::cos(value));
        }
        if (symbol == "tan" || symbol == "tanf") {
            return make_real(std::tan(value));
        }
        if (symbol == "exp" || symbol == "expf") {
            return make_real(std::exp(value));
        }
        if (symbol == "log" || symbol == "logf") {
            return make_real(std::log(value));
        }
        if (symbol == "round" || symbol == "roundf") {
            return make_real(std::round(value));
        }
        if (symbol == "floor" || symbol == "floorf") {
            return make_real(std::floor(value));
        }
        if (symbol == "ceil" || symbol == "ceilf") {
            return make_real(std::ceil(value));
        }
        if (symbol == "fabs" || symbol == "fabsf") {
            return make_real(std::fabs(value));
        }
    }

    if (arguments.size() == 2 && (symbol == "pow" || symbol == "powf")) {
        return make_real(std::pow(numeric_argument(arguments[0]), numeric_argument(arguments[1])));
    }

    throw std::runtime_error("unsupported foreign function '" + symbol + "'");
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
