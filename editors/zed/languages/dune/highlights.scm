[
  "as"
  "break"
  "const"
  "continue"
  "else"
  "enum"
  "export"
  "extern"
  "fn"
  "for"
  "if"
  "impl"
  "import"
  "let"
  "match"
  "print"
  "return"
  "struct"
  "while"
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

(extern_function_declaration
  name: (identifier) @function)

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

(struct_field
  name: (identifier) @property)

(enum_variant
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
  "="
  "=>"
  "->"
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
