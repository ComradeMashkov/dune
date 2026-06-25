(function_declaration
  "fn" @context
  name: (identifier) @name) @item

(foreign_function_declaration
  "foreign" @context
  "fn" @context
  name: (identifier) @name) @item

(record_declaration
  "record" @context
  name: (identifier) @name) @item

(choice_declaration
  "choice" @context
  name: (identifier) @name) @item

(choice_variant
  name: (identifier) @name) @item

(contract_declaration
  "contract" @context
  name: (identifier) @name) @item

(contract_method
  name: (identifier) @name) @item

(type_alias_declaration
  "type" @context
  name: (identifier) @name) @item

(method_declaration
  "method" @context
  name: (identifier) @name) @item

(const_statement
  "const" @context
  name: (identifier) @name) @item
