[
  "break"
  "choice"
  "const"
  "continue"
  "contract"
  "else"
  "export"
  "foreign"
  "for"
  "if"
  "import"
  "in"
  "is"
  "method"
  "print"
  "record"
  "return"
  "to"
  "when"
  "while"
  "with"
] @keyword

[
  "true"
  "false"
] @boolean

[
  "int"
  "bool"
  "i8"
  "i16"
  "i32"
  "i64"
  "isize"
  "u8"
  "u16"
  "u32"
  "u64"
  "usize"
  "uint8"
  "uint16"
  "uint32"
  "uint64"
  "real"
  "real32"
  "real64"
  "glyph"
  "text"
  "unit"
] @type.builtin

(comment) @comment
(number) @number
(float) @number.float
(string) @string
(character) @string.special

(function_declaration
  name: (identifier) @function)

(foreign_function_declaration
  name: (identifier) @function)

(contract_declaration
  name: (identifier) @type)

(contract_method
  name: (identifier) @function.method)

(method_declaration
  name: (identifier) @function.method)

(parameter
  name: (identifier) @variable.parameter)

(call_expression
  function: (identifier) @function)

(method_call_expression
  method: (identifier) @function.method)

(member_expression
  property: (identifier) @property)

(field_initializer
  name: (identifier) @property)

(record_field
  name: (identifier) @property)

(choice_variant
  name: (identifier) @variant)

(identifier) @variable

(constructor_identifier) @constructor

(identifier) @constructor
  (#match? @constructor "^[A-Z]")

[
  "+"
  "-"
  "*"
  "/"
  "%"
  "=="
  "!="
  ">"
  ">="
  "<"
  "<="
  "&&"
  "||"
  "!"
  ".."
  "="
] @operator

[
  "."
  ","
  ":"
  ";"
] @punctuation.delimiter

[
  "("
  ")"
  "{"
  "}"
  "["
  "]"
] @punctuation.bracket
