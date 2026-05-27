const PREC = {
  call: 12,
  member: 11,
  unary: 10,
  cast: 9,
  multiplicative: 8,
  additive: 7,
  comparison: 6,
  equality: 5,
  and: 4,
  or: 3,
};

module.exports = grammar({
  name: "dune",

  extras: $ => [
    /\s/,
    $.comment,
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat($._statement),

    comment: _ => token(seq("//", /.*/)),

    _statement: $ => choice(
      $.export_statement,
      $.import_statement,
      $.function_declaration,
      $.extern_function_declaration,
      $.struct_declaration,
      $.enum_declaration,
      $.impl_declaration,
      $.let_statement,
      $.const_statement,
      $.assignment_statement,
      $.print_statement,
      $.return_statement,
      $.if_statement,
      $.while_statement,
      $.for_statement,
      $.break_statement,
      $.continue_statement,
      $.block,
      $.expression_statement,
    ),

    export_statement: $ => seq(
      "export",
      choice(
        $.function_declaration,
        $.extern_function_declaration,
        $.struct_declaration,
        $.enum_declaration,
        $.impl_declaration,
        $.const_statement,
      ),
    ),

    import_statement: $ => seq("import", field("module", $.identifier), ";"),

    function_declaration: $ => seq(
      "fn",
      field("name", $.identifier),
      optional($.generic_parameters),
      field("parameters", $.parameter_list),
      optional(seq("->", field("return_type", $._type))),
      field("body", $.block),
    ),

    extern_function_declaration: $ => seq(
      "extern",
      "fn",
      field("name", $.identifier),
      optional($.generic_parameters),
      field("parameters", $.parameter_list),
      optional(seq("->", field("return_type", $._type))),
      "=",
      field("symbol", $.string),
      ";",
    ),

    struct_declaration: $ => seq(
      "struct",
      field("name", $.identifier),
      optional($.generic_parameters),
      "{",
      optional(commaSep($.struct_field)),
      optional(","),
      "}",
    ),

    struct_field: $ => seq(field("name", $.identifier), ":", field("type", $._type)),

    enum_declaration: $ => seq(
      "enum",
      field("name", $.identifier),
      optional($.generic_parameters),
      "{",
      optional(commaSep($.enum_variant)),
      optional(","),
      "}",
    ),

    enum_variant: $ => seq(
      field("name", $.identifier),
      optional(seq("(", field("payload", $._type), ")")),
    ),

    impl_declaration: $ => seq(
      "impl",
      optional($.generic_parameters),
      field("receiver", $._type),
      field("body", $.block),
    ),

    generic_parameters: $ => seq("<", commaSep1($.generic_parameter), ">"),

    generic_parameter: $ => seq(
      field("name", $.identifier),
      optional(seq(":", field("bound", $.identifier))),
    ),

    parameter_list: $ => seq("(", optional(commaSep($.parameter)), optional(","), ")"),

    parameter: $ => seq(
      field("name", $.identifier),
      optional(seq(":", field("type", $._type))),
    ),

    let_statement: $ => seq(
      "let",
      field("name", $.identifier),
      optional(seq(":", field("type", $._type))),
      "=",
      field("value", $._expression),
      ";",
    ),

    const_statement: $ => seq(
      "const",
      field("name", $.identifier),
      optional(seq(":", field("type", $._type))),
      "=",
      field("value", $._expression),
      ";",
    ),

    assignment_statement: $ => seq(
      field("target", choice($.identifier, $.member_expression, $.index_expression)),
      "=",
      field("value", $._expression),
      ";",
    ),

    print_statement: $ => seq("print", "(", field("value", $._expression), ")", ";"),

    return_statement: $ => seq("return", optional($._expression), ";"),

    break_statement: _ => seq("break", ";"),

    continue_statement: _ => seq("continue", ";"),

    if_statement: $ => seq(
      "if",
      field("condition", $._expression),
      field("consequence", $.block),
      optional(seq("else", field("alternative", choice($.block, $.if_statement)))),
    ),

    while_statement: $ => seq(
      "while",
      field("condition", $._expression),
      field("body", $.block),
    ),

    for_statement: $ => seq(
      "for",
      optional(choice($.for_let_initializer, $.for_assignment_initializer)),
      ";",
      optional(field("condition", $._expression)),
      ";",
      optional(field("increment", $.for_assignment_initializer)),
      field("body", $.block),
    ),

    for_let_initializer: $ => seq(
      "let",
      field("name", $.identifier),
      optional(seq(":", field("type", $._type))),
      "=",
      field("value", $._expression),
    ),

    for_assignment_initializer: $ => seq(
      field("target", choice($.identifier, $.member_expression, $.index_expression)),
      "=",
      field("value", $._expression),
    ),

    block: $ => seq("{", repeat($._statement), "}"),

    expression_statement: $ => seq($._expression, ";"),

    _type: $ => choice(
      $.array_type,
      $.generic_type,
      $.builtin_type,
      $.qualified_type_identifier,
      $.identifier,
    ),

    array_type: $ => seq("[", field("element", $._type), "]"),

    generic_type: $ => prec(1, seq(
      field("name", choice($.qualified_type_identifier, $.identifier)),
      "<",
      commaSep1(field("argument", $._type)),
      ">",
    )),

    builtin_type: _ => choice(
      "int",
      "bool",
      "i8",
      "i16",
      "i32",
      "i64",
      "isize",
      "u8",
      "u16",
      "u32",
      "u64",
      "usize",
      "uint8",
      "uint16",
      "uint32",
      "uint64",
      "real",
      "real32",
      "real64",
      "glyph",
      "text",
      "unit",
    ),

    _expression: $ => choice(
      $.match_expression,
      $.method_call_expression,
      $.member_expression,
      $.call_expression,
      $.index_expression,
      $.slice_expression,
      $.binary_expression,
      $.unary_expression,
      $.cast_expression,
      $._primary_expression,
    ),

    _primary_expression: $ => choice(
      $.array_literal,
      $.struct_literal,
      $.parenthesized_expression,
      $.number,
      $.float,
      $.character,
      $.string,
      $.boolean,
      $.constructor_identifier,
      $.identifier,
    ),

    parenthesized_expression: $ => seq("(", $._expression, ")"),

    match_expression: $ => seq(
      "match",
      field("value", $._expression),
      "{",
      repeat(seq($.match_arm, optional(","))),
      "}",
    ),

    match_arm: $ => seq(field("pattern", $._pattern), "=>", field("body", $._expression)),

    _pattern: $ => choice(
      $.wildcard_pattern,
      $.variant_pattern,
      $.number,
      $.float,
      $.character,
      $.string,
      $.boolean,
      $.identifier,
    ),

    wildcard_pattern: _ => "_",

    variant_pattern: $ => prec(1, seq(
      field("name", choice($.identifier, $.member_expression)),
      "(",
      optional(choice($.identifier, $.wildcard_pattern)),
      ")",
    )),

    array_literal: $ => seq("[", optional(commaSep($._expression)), optional(","), "]"),

    struct_literal: $ => prec(2, seq(
      field("type", choice($.qualified_type_identifier, $.constructor_identifier)),
      "{",
      optional(commaSep($.field_initializer)),
      optional(","),
      "}",
    )),

    field_initializer: $ => seq(field("name", $.identifier), ":", field("value", $._expression)),

    call_expression: $ => prec(PREC.call, seq(
      field("function", choice($.identifier, $.constructor_identifier)),
      field("arguments", $.argument_list),
    )),

    method_call_expression: $ => prec.left(PREC.call, seq(
      field("receiver", choice($.identifier, $.member_expression, $.call_expression, $.index_expression, $.parenthesized_expression)),
      ".",
      field("method", choice($.identifier, $.constructor_identifier)),
      field("arguments", $.argument_list),
    )),

    argument_list: $ => seq("(", optional(commaSep($._expression)), optional(","), ")"),

    member_expression: $ => prec.left(PREC.member, seq(
      field("receiver", $.identifier),
      repeat(seq(".", $.identifier)),
      ".",
      field("property", choice($.identifier, $.constructor_identifier)),
    )),

    index_expression: $ => prec.left(PREC.member, seq(
      field("receiver", choice($.identifier, $.member_expression, $.call_expression, $.parenthesized_expression)),
      "[",
      field("index", $._expression),
      "]",
    )),

    slice_expression: $ => prec.left(PREC.member, seq(
      field("receiver", choice($.identifier, $.member_expression, $.call_expression, $.parenthesized_expression)),
      "[",
      optional(field("start", $._expression)),
      ":",
      optional(field("end", $._expression)),
      "]",
    )),

    unary_expression: $ => prec(PREC.unary, seq(
      field("operator", choice("-", "!")),
      field("argument", $._expression),
    )),

    cast_expression: $ => prec.left(PREC.cast, seq(
      field("value", $._expression),
      "as",
      field("type", $._type),
    )),

    binary_expression: $ => choice(
      ...[
        ["||", PREC.or],
        ["&&", PREC.and],
        ["==", PREC.equality],
        ["!=", PREC.equality],
        [">", PREC.comparison],
        [">=", PREC.comparison],
        ["<", PREC.comparison],
        ["<=", PREC.comparison],
        ["+", PREC.additive],
        ["-", PREC.additive],
        ["*", PREC.multiplicative],
        ["/", PREC.multiplicative],
        ["%", PREC.multiplicative],
      ].map(([operator, precedence]) =>
        prec.left(precedence, seq(
          field("left", $._expression),
          field("operator", operator),
          field("right", $._expression),
        )),
      ),
    ),

    boolean: _ => choice("true", "false"),

    identifier: _ => /[A-Za-z_][A-Za-z0-9_]*/,

    qualified_type_identifier: $ => prec(PREC.member + 1, seq($.identifier, ".", $.constructor_identifier)),

    constructor_identifier: _ => token(prec(1, /[A-Z][A-Za-z0-9_]*/)),

    number: _ => /\d+/,

    float: _ => /\d+\.\d+/,

    character: _ => token(seq("'", choice(/[^'\\\n]/, seq("\\", /./)), "'")),

    string: _ => token(seq('"', repeat(choice(/[^"\\\n]/, seq("\\", /./))), '"')),
  },
});

function commaSep(rule) {
  return optional(commaSep1(rule));
}

function commaSep1(rule) {
  return seq(rule, repeat(seq(",", rule)));
}
