#include "vm.hpp"

#include "native_canvas_display.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
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

std::string lowercase_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

int parse_log_level(const char* value) {
    if (value == nullptr || *value == '\0') {
        return 2;
    }

    const std::string level = lowercase_ascii(value);
    if (level == "trace" || level == "0") {
        return 0;
    }
    if (level == "debug" || level == "1") {
        return 1;
    }
    if (level == "info" || level == "2") {
        return 2;
    }
    if (level == "warn" || level == "warning" || level == "3") {
        return 3;
    }
    if (level == "error" || level == "4") {
        return 4;
    }
    if (level == "off" || level == "none" || level == "quiet" || level == "5") {
        return 5;
    }

    return 2;
}

int initial_log_level() {
    const char* level = std::getenv("DUNE_LOG");
    if (level != nullptr && *level != '\0') {
        return parse_log_level(level);
    }

    return parse_log_level(std::getenv("DUNE_LOG_LEVEL"));
}

Value make_array(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::array;
    result.array_value = std::make_shared<std::vector<Value>>(std::move(values));
    return result;
}

Value make_tuple(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::tuple;
    result.tuple_value = std::make_shared<std::vector<Value>>(std::move(values));
    return result;
}

Value make_record(std::vector<Value> values) {
    Value result;
    result.kind = ValueKind::record;
    result.record_value = std::make_shared<std::vector<Value>>(std::move(values));
    return result;
}

Value make_variant(std::size_t tag, std::string name, std::shared_ptr<Value> payload) {
    Value result;
    result.kind = ValueKind::variant;
    result.variant_tag = tag;
    result.variant_name = std::move(name);
    result.variant_payload = std::move(payload);
    return result;
}

Value make_callable(std::size_t function_index, std::vector<Value> captures = {}) {
    Value result;
    result.kind = ValueKind::callable;
    result.function_index = function_index;
    result.closure_captures = std::make_shared<std::vector<Value>>(std::move(captures));
    return result;
}

void expect_same_kind(const Value& left, const Value& right) {
    if (left.kind != right.kind) {
        throw std::runtime_error("runtime type mismatch");
    }
}

Value add_values(const Value& left, const Value& right) {
    // Text concatenation: `text + text` joins two strings and a `glyph` on
    // either side appends a single character. Handle it before the same-kind
    // check so the mixed text/glyph forms are allowed.
    if (left.kind == ValueKind::text) {
        if (right.kind == ValueKind::text) {
            return make_text(left.text_value + right.text_value);
        }
        if (right.kind == ValueKind::glyph) {
            return make_text(left.text_value + right.glyph_value);
        }
    } else if (left.kind == ValueKind::glyph && right.kind == ValueKind::text) {
        return make_text(left.glyph_value + right.text_value);
    }

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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
        throw std::runtime_error("cannot format unit value");
    case ValueKind::array:
        throw std::runtime_error("cannot format array value");
    case ValueKind::tuple:
        throw std::runtime_error("cannot format tuple value");
    case ValueKind::record:
        throw std::runtime_error("cannot format record value");
    case ValueKind::variant:
        if (value.variant_payload == nullptr) {
            return value.variant_name;
        }
        return value.variant_name + "(" + value_to_text(*value.variant_payload) + ")";
    case ValueKind::callable:
        throw std::runtime_error("cannot format function value");
    }

    throw std::runtime_error("cannot format unknown value");
}

std::string format_value(const std::string& format, const std::vector<Value>& arguments) {
    std::ostringstream output;
    std::size_t argument_index = 0;
    for (std::size_t index = 0; index < format.size(); ++index) {
        if (format[index] == '{' && index + 1 < format.size() && format[index + 1] == '}') {
            if (argument_index >= arguments.size()) {
                throw std::runtime_error("not enough format arguments");
            }

            output << value_to_text(arguments[argument_index++]);
            ++index;
            continue;
        }

        output << format[index];
    }

    if (argument_index != arguments.size()) {
        throw std::runtime_error("too many format arguments");
    }

    return output.str();
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

std::vector<Value>& tuple_elements(const Value& value) {
    if (value.kind != ValueKind::tuple || value.tuple_value == nullptr) {
        throw std::runtime_error("expected tuple value");
    }

    return *value.tuple_value;
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
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
    case ValueKind::tuple:
    case ValueKind::record:
    case ValueKind::variant:
    case ValueKind::callable:
        break;
    }

    throw std::runtime_error("invalid glyph cast operand");
}

} // namespace

VirtualMachine::VirtualMachine(Bytecode bytecode) : bytecode_(std::move(bytecode)), log_level_(initial_log_level()) {}

VirtualMachine::VirtualMachine(Bytecode bytecode, std::vector<std::string> program_arguments)
    : bytecode_(std::move(bytecode)), program_arguments_(std::move(program_arguments)), log_level_(initial_log_level()) {
}

void VirtualMachine::run(std::ostream& output) {
    run(output, std::cerr, std::cin);
}

void VirtualMachine::run(std::ostream& output, std::ostream& error, std::istream& input) {
    stack_.clear();
    frames_.clear();
    frames_.push_back(CallFrame{&bytecode_.instructions, 0, std::vector<Value>(bytecode_.local_count), 0});
    execute(output, error, input);
}

// Runs a single compiled test chunk to completion. The caller (`dune test`)
// wraps this in try/catch: a failed assertion aborts via `runtime.panic`
// (a thrown std::runtime_error), which unwinds out of here and marks the test
// failed without killing the process.
void VirtualMachine::run_test(std::size_t function_index, std::ostream& output) {
    const Bytecode::Function& function = bytecode_.functions.at(function_index);
    stack_.clear();
    frames_.clear();
    frames_.push_back(CallFrame{&function.instructions, 0, std::vector<Value>(function.local_count), 0});
    execute(output, std::cerr, std::cin);
}

void VirtualMachine::execute(std::ostream& output, std::ostream& error, std::istream& input) {
    try {
        execute_until(output, error, input, 0);
    } catch (const std::exception& exception) {
        const std::string primary_error = exception.what();
        std::vector<std::string> cleanup_errors;
        unwind_frames_to(0, output, error, input, cleanup_errors);

        std::string message = primary_error;
        for (const std::string& cleanup_error : cleanup_errors) {
            message += "\nwhile running deferred cleanup: " + cleanup_error;
        }
        throw std::runtime_error(message);
    }
}

void VirtualMachine::execute_until(std::ostream& output, std::ostream& error, std::istream& input,
                                   std::size_t frame_depth) {
    while (frames_.size() > frame_depth) {
        CallFrame& frame = frames_.back();
        if (frame.ip >= frame.instructions->size()) {
            throw std::runtime_error("instruction pointer moved past the end of a function");
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
        case OpCode::push_function:
            stack_.push_back(make_callable(instruction.operand));
            ++frame.ip;
            break;
        case OpCode::make_closure: {
            const Bytecode::Function& function = bytecode_.functions.at(instruction.operand);
            if (stack_.size() < function.capture_count) {
                throw std::runtime_error("not enough captured values on stack for closure");
            }
            std::vector<Value> captures(function.capture_count);
            for (std::size_t index = function.capture_count; index > 0; --index) {
                captures[index - 1] = pop();
            }
            stack_.push_back(make_callable(instruction.operand, std::move(captures)));
            ++frame.ip;
            break;
        }
        case OpCode::call_value: {
            const Value callable = pop();
            // Increment before pushing the callee frame: call_callable may
            // reallocate frames_ and invalidate `frame`.
            ++frame.ip;
            call_callable(callable);
            break;
        }
        case OpCode::defer_scope_enter:
            frame.defer_scope_starts.push_back(frame.deferred_calls.size());
            ++frame.ip;
            break;
        case OpCode::defer_push: {
            const Value callable = pop();
            if (callable.kind != ValueKind::callable) {
                throw std::runtime_error("defer expects a callable cleanup");
            }
            frame.deferred_calls.push_back(callable);
            ++frame.ip;
            break;
        }
        case OpCode::defer_scope_exit: {
            if (frame.awaiting_defer_result) {
                pop();
                frame.awaiting_defer_result = false;
            }
            if (frame.defer_scope_starts.empty()) {
                throw std::runtime_error("defer scope stack underflow");
            }

            const std::size_t scope_start = frame.defer_scope_starts.back();
            if (frame.deferred_calls.size() > scope_start) {
                const Value callable = frame.deferred_calls.back();
                frame.deferred_calls.pop_back();
                frame.awaiting_defer_result = true;
                call_callable(callable);
                break;
            }

            frame.defer_scope_starts.pop_back();
            ++frame.ip;
            break;
        }
        case OpCode::return_value: {
            if (frame.awaiting_defer_result) {
                pop();
                frame.awaiting_defer_result = false;
            }
            if (!frame.pending_return.has_value()) {
                frame.pending_return = pop();
            }
            if (!frame.deferred_calls.empty()) {
                const Value callable = frame.deferred_calls.back();
                frame.deferred_calls.pop_back();
                frame.awaiting_defer_result = true;
                call_callable(callable);
                break;
            }

            const Value result = *frame.pending_return;
            const std::size_t base = frames_.back().stack_base;
            frames_.pop_back();

            // Discard any operands the returning frame left on the shared stack
            // (an early `?` return can unwind mid-expression with values still pushed).
            if (stack_.size() > base) {
                stack_.resize(base);
            }

            if (!frames_.empty()) {
                stack_.push_back(result);
            }
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
        case OpCode::make_tuple: {
            if (stack_.size() < instruction.operand) {
                throw std::runtime_error("not enough values on stack for tuple literal");
            }

            std::vector<Value> elements(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                elements[index - 1] = pop();
            }

            stack_.push_back(make_tuple(std::move(elements)));
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
            // Stack layout (see compile_variant_constructor): [payload, name] with
            // the variant name on top.
            std::string name = pop().text_value;
            Value payload = pop();
            stack_.push_back(make_variant(instruction.operand, std::move(name), std::make_shared<Value>(std::move(payload))));
            ++frame.ip;
            break;
        }
        case OpCode::make_unit_variant: {
            std::string name = pop().text_value;
            stack_.push_back(make_variant(instruction.operand, std::move(name), nullptr));
            ++frame.ip;
            break;
        }
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
        case OpCode::load_tuple_element: {
            const Value tuple = pop();
            std::vector<Value>& elements = tuple_elements(tuple);
            if (instruction.operand >= elements.size()) {
                throw std::runtime_error("tuple element out of bounds");
            }

            stack_.push_back(elements[instruction.operand]);
            ++frame.ip;
            break;
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
        case OpCode::array_contains: {
            const Value array = pop();
            const Value needle = pop();
            bool found = false;
            for (const Value& element : array_elements(array)) {
                if (values_equal(element, needle)) {
                    found = true;
                    break;
                }
            }
            stack_.push_back(make_bool(found));
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
        case OpCode::text_in: {
            const Value text = pop();
            const Value needle = pop();
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
        case OpCode::read_file: {
            const Value path = pop();
            if (path.kind != ValueKind::text) {
                throw std::runtime_error("read_file expects a text path");
            }

            std::ifstream input(path.text_value, std::ios::binary);
            std::vector<Value> result(2);
            if (!input) {
                result[0] = make_bool(false);
                result[1] = make_text("could not open '" + path.text_value + "' for reading");
            } else {
                std::ostringstream buffer;
                buffer << input.rdbuf();
                result[0] = make_bool(true);
                result[1] = make_text(buffer.str());
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::write_file: {
            const Value content = pop();
            const Value path = pop();
            if (path.kind != ValueKind::text || content.kind != ValueKind::text) {
                throw std::runtime_error("write_file expects text path and content");
            }

            std::ofstream file(path.text_value, std::ios::binary | std::ios::trunc);
            std::vector<Value> result(2);
            if (!file) {
                result[0] = make_bool(false);
                result[1] = make_text("could not open '" + path.text_value + "' for writing");
            } else {
                file << content.text_value;
                if (!file) {
                    result[0] = make_bool(false);
                    result[1] = make_text("could not write to '" + path.text_value + "'");
                } else {
                    result[0] = make_bool(true);
                    result[1] = make_text("");
                }
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::stdout_write: {
            const Value text = pop();
            if (text.kind != ValueKind::text) {
                throw std::runtime_error("stdout_write expects text");
            }

            output << text.text_value;
            std::vector<Value> result(2);
            if (output) {
                result[0] = make_bool(true);
                result[1] = make_text("");
            } else {
                result[0] = make_bool(false);
                result[1] = make_text("could not write to stdout");
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::stderr_write: {
            const Value text = pop();
            if (text.kind != ValueKind::text) {
                throw std::runtime_error("stderr_write expects text");
            }

            error << text.text_value;
            std::vector<Value> result(2);
            if (error) {
                result[0] = make_bool(true);
                result[1] = make_text("");
            } else {
                result[0] = make_bool(false);
                result[1] = make_text("could not write to stderr");
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::stdout_flush: {
            output.flush();
            std::vector<Value> result(2);
            if (output) {
                result[0] = make_bool(true);
                result[1] = make_text("");
            } else {
                result[0] = make_bool(false);
                result[1] = make_text("could not flush stdout");
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::stderr_flush: {
            error.flush();
            std::vector<Value> result(2);
            if (error) {
                result[0] = make_bool(true);
                result[1] = make_text("");
            } else {
                result[0] = make_bool(false);
                result[1] = make_text("could not flush stderr");
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::stdin_read_line: {
            std::string line;
            std::vector<Value> result(2);
            if (std::getline(input, line)) {
                result[0] = make_bool(true);
                result[1] = make_text(std::move(line));
            } else if (input.eof()) {
                result[0] = make_bool(false);
                result[1] = make_text("end of input");
            } else {
                result[0] = make_bool(false);
                result[1] = make_text("could not read from stdin");
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::env_get: {
            const Value name = pop();
            if (name.kind != ValueKind::text) {
                throw std::runtime_error("env_get expects a text name");
            }

            const char* value = std::getenv(name.text_value.c_str());
            std::vector<Value> result(2);
            result[0] = make_bool(value != nullptr);
            result[1] = make_text(value == nullptr ? std::string() : std::string(value));
            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::process_args: {
            std::vector<Value> values;
            values.reserve(program_arguments_.size());
            for (const std::string& argument : program_arguments_) {
                values.push_back(make_text(argument));
            }

            stack_.push_back(make_array(std::move(values)));
            ++frame.ip;
            break;
        }
        case OpCode::process_cwd: {
            std::vector<Value> result(2);
            std::error_code error;
            const std::filesystem::path directory = std::filesystem::current_path(error);
            if (error) {
                result[0] = make_bool(false);
                result[1] = make_text(error.message());
            } else {
                result[0] = make_bool(true);
                result[1] = make_text(directory.string());
            }

            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::log_emit: {
            const Value message = pop();
            const Value label = pop();
            const Value level = pop();
            if (level.kind != ValueKind::signed_integer) {
                throw std::runtime_error("log_emit expects an integer level");
            }
            if (label.kind != ValueKind::text) {
                throw std::runtime_error("log_emit expects a text label");
            }
            if (message.kind != ValueKind::text) {
                throw std::runtime_error("log_emit expects a text message");
            }

            if (level.signed_value >= log_level_) {
                error << '[' << label.text_value << "] " << message.text_value << '\n';
            }
            stack_.push_back(make_unit());
            ++frame.ip;
            break;
        }
        case OpCode::log_set_level: {
            const Value level = pop();
            if (level.kind != ValueKind::signed_integer) {
                throw std::runtime_error("log_set_level expects an integer level");
            }

            log_level_ = static_cast<int>(level.signed_value);
            stack_.push_back(make_unit());
            ++frame.ip;
            break;
        }
        case OpCode::log_level:
            stack_.push_back(make_signed(log_level_));
            ++frame.ip;
            break;
        case OpCode::plot_backend_get:
            stack_.push_back(make_text(plot_backend_));
            ++frame.ip;
            break;
        case OpCode::plot_backend_set: {
            const Value name = pop();
            if (name.kind != ValueKind::text) {
                throw std::runtime_error("plot_backend_set expects a text name");
            }

            plot_backend_ = name.text_value;
            stack_.push_back(make_unit());
            ++frame.ip;
            break;
        }
        case OpCode::plot_show_native: {
            const Value svg = pop();
            if (svg.kind != ValueKind::text) {
                throw std::runtime_error("plot_show_native expects SVG text");
            }

            const NativeCanvasDisplayResult display = show_native_canvas_svg("Dune Plot", svg.text_value);
            std::vector<Value> result(2);
            result[0] = make_bool(display.ok);
            result[1] = make_text(display.message);
            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::canvas_show_native: {
            const Value svg = pop();
            const Value title = pop();
            if (title.kind != ValueKind::text || svg.kind != ValueKind::text) {
                throw std::runtime_error("canvas_show_native expects text title and SVG text");
            }

            const NativeCanvasDisplayResult display = show_native_canvas_svg(title.text_value, svg.text_value);
            std::vector<Value> result(2);
            result[0] = make_bool(display.ok);
            result[1] = make_text(display.message);
            stack_.push_back(make_tuple(std::move(result)));
            ++frame.ip;
            break;
        }
        case OpCode::format_text: {
            std::vector<Value> arguments(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                arguments[index - 1] = pop();
            }

            const Value format = pop();
            if (format.kind != ValueKind::text) {
                throw std::runtime_error("format string must be text");
            }

            stack_.push_back(make_text(format_value(format.text_value, arguments)));
            ++frame.ip;
            break;
        }
        case OpCode::repl_print:
            output << value_to_text(pop()) << '\n';
            ++frame.ip;
            break;
        case OpCode::halt: {
            if (frame.awaiting_defer_result) {
                pop();
                frame.awaiting_defer_result = false;
            }
            if (!frame.deferred_calls.empty()) {
                const Value callable = frame.deferred_calls.back();
                frame.deferred_calls.pop_back();
                frame.awaiting_defer_result = true;
                call_callable(callable);
                break;
            }

            const std::size_t base = frame.stack_base;
            frames_.pop_back();
            if (stack_.size() > base) {
                stack_.resize(base);
            }
            break;
        }
        }
    }
}

void VirtualMachine::call_callable(const Value& callable) {
    if (callable.kind != ValueKind::callable) {
        throw std::runtime_error("attempted to call a non-function value");
    }

    static const std::vector<Value> empty_captures;
    call_function(callable.function_index,
                  callable.closure_captures != nullptr ? *callable.closure_captures : empty_captures);
}

void VirtualMachine::unwind_frames_to(std::size_t frame_depth, std::ostream& output, std::ostream& error,
                                      std::istream& input, std::vector<std::string>& cleanup_errors) {
    while (frames_.size() > frame_depth) {
        const std::size_t owner_depth = frames_.size();
        const std::size_t stack_base = frames_.back().stack_base;
        if (stack_.size() > stack_base) {
            stack_.resize(stack_base);
        }

        while (!frames_.back().deferred_calls.empty()) {
            const Value callable = frames_.back().deferred_calls.back();
            frames_.back().deferred_calls.pop_back();

            try {
                call_callable(callable);
                if (frames_.size() > owner_depth) {
                    execute_until(output, error, input, owner_depth);
                }
                if (stack_.size() > stack_base) {
                    stack_.resize(stack_base);
                }
            } catch (const std::exception& exception) {
                const std::string cleanup_error = exception.what();
                if (frames_.size() > owner_depth) {
                    unwind_frames_to(owner_depth, output, error, input, cleanup_errors);
                }
                if (stack_.size() > stack_base) {
                    stack_.resize(stack_base);
                }
                cleanup_errors.push_back(cleanup_error);
            }
        }

        frames_.pop_back();
        if (stack_.size() > stack_base) {
            stack_.resize(stack_base);
        }
    }
}

void VirtualMachine::call_function(std::size_t function_index, const std::vector<Value>& captures) {
    const Bytecode::Function& function = bytecode_.functions.at(function_index);
    if (stack_.size() < function.arity) {
        throw std::runtime_error("not enough arguments on stack for function call");
    }

    if (function.is_extern) {
        if (!captures.empty()) {
            throw std::runtime_error("foreign functions cannot have closure captures");
        }
        std::vector<Value> arguments(function.arity);
        for (std::size_t index = function.arity; index > 0; --index) {
            arguments[index - 1] = pop();
        }

        stack_.push_back(call_extern_function(function, std::move(arguments)));
        return;
    }

    if (captures.size() != function.capture_count) {
        throw std::runtime_error("closure capture count mismatch");
    }

    std::vector<Value> locals(function.local_count);
    for (std::size_t index = function.arity; index > 0; --index) {
        locals[index - 1] = pop();
    }
    for (std::size_t index = 0; index < captures.size(); ++index) {
        locals[function.arity + index] = captures[index];
    }

    const std::size_t base = stack_.size();
    frames_.push_back(CallFrame{&function.instructions, 0, std::move(locals), base});
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
