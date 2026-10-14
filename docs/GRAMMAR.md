# Grammar

Status: Phase 0 design, revision 2. EBNF over lexer tokens; braces mean repetition, brackets optional, and | alternatives. Spaces/comments omitted; NEWLINE is significant at top-level expression nesting. Static constraints follow the productions.

## Lexical tokens

    IDENT       = letter, { letter | digit | "_" } ;
    INT         = digit, { digit } ;
    FLOAT       = digit, { digit }, ".", digit, { digit }, [ exponent ]
                | digit, { digit }, exponent ;
    exponent    = ("e" | "E"), [ "+" | "-" ], digit, { digit } ;
    STRING      = U+0022, { string_char | escape | interpolation
                | escaped_brace }, U+0022 ;
    interpolation = "{", expression, "}" ;
    escaped_brace = "{{" | "}}" ;
    escape      = U+005C, ( U+005C | U+0022 | "n" | "r" | "t" | "0" ) ;
    CHAR        = U+0027, char_scalar, U+0027 ;
    letter      = "A" … "Z" | "a" … "z" | "_" ;
    digit       = "0" … "9" ;

Longest-match operators. Keywords are specified in SPEC.md; and/or/not are word operators. Comments start with //. Strings are UTF-8; string_char excludes quote, backslash, and braces so doubled braces and interpolation are unambiguous. Trailing commas are permitted in every delimited list. Numeric suffixes and separators are not in this grammar revision. At parenthesis/bracket depth zero, the lexer emits NEWLINE; nested newlines are insignificant. Newline terminates only a complete statement when the next token cannot continue its expression. Newline before else is ignored. Semicolon is an explicit terminator.

## Declarations and types

    program         = { declaration | statement }, EOF ;
    declaration     = import_decl | function_decl | class_decl | interface_decl
                    | type_decl | enum_decl | variable_decl ;
    import_decl     = "import", module_path, [ "as", IDENT ], terminator ;
    module_path     = IDENT, { ".", IDENT } ;
    variable_decl   = ("let" | "var"), pattern, [ ":", type ], "=", expression,
                      terminator ;
    function_decl   = { attribute }, [ visibility ], "fn", IDENT, [ generic_params ],
                      "(", [ parameter_list ], ")", [ "->", type ],
                      ( block | "=", expression, terminator ) ;
    generic_params  = "[", IDENT, { ",", IDENT }, [ "," ], "]" ;
    parameter_list  = parameter, { ",", parameter }, [ "," ] ;
    parameter       = [ "..." ], [ IDENT, ":" ], IDENT, ":", type,
                      [ "=", expression ] ;
    type_decl       = "type", IDENT, [ generic_params ], "=", variant,
                      { "|", variant } ;
    variant         = IDENT, [ "(", type_list, [ "," ], ")" ] ;
    enum_decl       = { attribute }, [ visibility ], "enum", IDENT,
                      "{", [ enum_item, { enum_separator, enum_item },
                      [ enum_separator ] ], "}" ;
    enum_item       = enum_case | method_decl ;
    enum_case       = IDENT, [ "(", type_list, [ "," ], ")" ] ;
    enum_separator  = "," | NEWLINE ;
    interface_decl  = { attribute }, [ visibility ], "interface", IDENT,
                      [ generic_params ], "{", { interface_member }, "}" ;
    interface_member = { attribute }, [ visibility ], function_signature ;
    function_signature = "fn", IDENT, [ generic_params ], "(",
                         [ parameter_list ], ")", [ "->", type ], terminator ;
    class_decl      = { attribute }, [ visibility ], "class", IDENT,
                      [ generic_params ], [ ":", type ],
                      "{", { class_member }, "}" ;
    visibility      = "public" | "private" | "protected" ;
    attribute       = "@", IDENT, [ "(", [ argument_list ], ")" ] ;
    type_list       = type, { ",", type } ;

    type            = union_type ;
    union_type      = postfix_type, { "|", postfix_type } ;
    postfix_type    = type_atom, { "[", type_list, "]" } ;
    type_atom       = IDENT | "_"
                    | "(", [ type_list ], ")", "->", type
                    | "(", type, ")" ;

## Classes and statements

    class_member    = { attribute }, [ visibility ], ( constructor | method_decl
                    | field_decl ) ;
    method_decl     = "fn", IDENT, [ generic_params ], "(",
                      [ parameter_list ], ")", [ "->", type ],
                      ( block | "=", expression, terminator ) ;
    constructor     = "init", "(", [ promoted_parameter_list ], ")",
                      ( block | "=", expression, terminator ) ;
    promoted_parameter_list = promoted_parameter, { ",", promoted_parameter },
                              [ "," ] ;
    promoted_parameter = "self", ".", IDENT, ":", type, [ "=", expression ]
                       | IDENT, ":", type, [ "=", expression ] ;
    field_decl      = ("let" | "var"), IDENT, ":", type, [ "=", expression ],
                      terminator ;
    block           = "{", { declaration | statement }, "}" ;
    statement       = block | if_stmt | while_stmt | for_stmt | return_stmt
                    | break_stmt | continue_stmt | expression_stmt ;
    if_stmt         = "if", expression, block, [ "else", ( block | if_stmt ) ] ;
    while_stmt      = "while", expression, block ;
    for_stmt        = "for", pattern, "in", expression, block ;
    expression_stmt = expression, terminator ;
    terminator      = ";" | NEWLINE, { NEWLINE } | implicit_before_close ;
    return_stmt     = "return", [ expression ], terminator ;
    break_stmt      = "break", terminator ;
    continue_stmt   = "continue", terminator ;
    implicit_before_close = ε ;

## Patterns and match

    pattern         = or_pattern ;
    or_pattern      = pattern_atom, { "|", pattern_atom } ;
    pattern_atom    = "_" | IDENT | literal
                    | IDENT, "(", [ pattern_list ], ")"
                    | "[", [ pattern_list ], "]"
                    | "(", pattern_list, ")"
                    | range_pattern ;
    pattern_list    = pattern_item, { ",", pattern_item }, [ "," ] ;
    pattern_item    = pattern | "...", IDENT ;
    range_pattern   = literal, (".." | "..="), literal ;
    match_expr      = "match", expression, "{", match_arm,
                      { match_separator, match_arm }, [ match_separator ], "}" ;
    match_separator = "," | NEWLINE ;
    match_arm       = pattern, { ",", pattern }, [ "if", expression ],
                      "=>", expression ;

Case patterns match ADT/enum variants; list/tuple patterns destructure; guards are Bool; arms do not fall through. Match is exhaustive and wildcard matches remaining values.

## Expressions

    expression      = assignment ;
    assignment      = place, assignment_op, assignment | "parallel", logic_or
                    | logic_or ;
    assignment_op   = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "**=" ;
    place           = IDENT, { member | index } ;
    logic_or        = logic_and, { "or", logic_and } ;
    logic_and       = equality, { "and", equality } ;
    equality        = comparison, { ("==" | "!="), comparison } ;
    comparison      = range_expr, [ ("<" | "<=" | ">" | ">="), range_expr ] ;
    range_expr      = matrix_expr, [ (".." | "..="), matrix_expr ] ;
    matrix_expr     = term, { "@", term } ;
    term            = factor, { ("+" | "-"), factor } ;
    factor          = unary, { ("*" | "/" | "%"), unary } ;
    unary           = ("not" | "+" | "-" | "spawn" | "await" | "try"), unary
                    | power ;
    power           = postfix, [ "**", unary ] ;
    postfix         = primary, { call | member | index } ;
    call            = "(", [ argument_list ], ")" ;
    member          = ".", IDENT ;
    index           = "[", expression, { ",", expression }, [ "," ], "]" ;
    primary         = literal | IDENT | lambda | match_expr | list_literal
                    | "(", expression, ",", [ expression_list ], ")"
                    | kernel_expr | "(", expression, ")" | block ;
    lambda          = "(", [ parameter_list ], ")", "=>", expression ;
    kernel_expr     = "kernel", "(", expression, ")", "{", IDENT, "=>",
                      expression, "}" ;
    list_literal    = "[", [ expression_list ], "]" ;
    expression_list = expression_item, { ",", expression_item }, [ "," ] ;
    expression_item = [ "..." ], [ IDENT, ":" ], expression
                    | expression, "for", pattern, "in", expression ;
    argument_list   = argument, { ",", argument }, [ "," ] ;
    argument        = [ IDENT, ":" ], expression ;
    literal         = INT | FLOAT | STRING | CHAR | "true" | "false" ;

Postfix calls/member/index bind tightest. Exponentiation is right-associative and tighter than unary minus. Other arithmetic/matrix operators associate left; comparisons and ranges are non-associative. Assignment is right-associative and targets a mutable place. In the STRING production, escaped braces are literal braces and interpolation braces delimit an expression; a brace cannot ambiguously begin both forms.

## Static constraints and parser

Grammar cannot encode name resolution, generic arity, mutability, type compatibility, exhaustiveness, effects, visibility, or device legality. Resolver and type checker enforce them before IR lowering. Named arguments bind by parameter name; duplicates are errors and positional arguments cannot follow named arguments.

Implement declarations/statements using recursive descent and expressions using Pratt parsing with binding powers matching this grammar. Interpolation tracks nested braces and string literals. Recover at semicolons, commas, closing delimiters, and declaration starters; always advance. AST nodes carry source spans; malformed nodes never reach lowering.
