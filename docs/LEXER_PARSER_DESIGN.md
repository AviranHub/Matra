# Lexer and Parser Design

Status: Phase 2 implementation contract.

## Shared ownership and source spans

SourceBuffer owns a NUL-terminated byte buffer and file name; its logical length excludes the terminator. Lexer tokens and AST text slices borrow this storage. The source must outlive token arrays and AST consumers. TokenArray owns only its token array; AstArena owns AST nodes and list links. Diagnostics own their messages. Destruction order is AST arena, token array, then source buffer.

Every token and AST node carries SourceSpan with half-open byte offsets and one-based start line/column. The lexer accepts a UTF-8 BOM only at byte offset zero, validates UTF-8 in strings/comments, and rejects non-ASCII identifier characters with a location. CRLF is one newline. Columns count Unicode scalar values; malformed sequences advance by one byte after reporting so lexing recovers.

## Source API

source.h exposes SourceBuffer; source_buffer_from_bytes(SourceBuffer *, const char *name, const unsigned char *bytes, size_t length, Diagnostics *); source_buffer_load(SourceBuffer *, const char *path, Diagnostics *); source_buffer_destroy(SourceBuffer *). Both constructors copy input so the buffer has a single clear owner. File loading uses binary mode and checked size growth. Complexity is O(n) time and O(n) owned bytes.

## Token API

token.h defines TokenKind for punctuation, operators, keywords, identifiers/literals, string/interpolation boundaries, NEWLINE, ERROR, and EOF; Token contains kind, SourceSpan, and borrowed source slice. TokenArray is a growable owned array. token_kind_name(TokenKind) returns a static name.

lexer.h exposes lexer_scan(const SourceBuffer *, TokenArray *, Diagnostics *), token_array_destroy(TokenArray *). Scanning emits recoverable ERROR tokens and diagnostics for source errors; false is reserved for invalid API arguments or allocation failure. Strings tokenize as STRING_BEGIN, zero or more STRING_TEXT/interpolation sequences, then STRING_END. Interpolation expressions use ordinary tokens and a matching INTERP_END. Newlines are emitted as tokens; parser rules ignore them inside delimited expressions and treat them as statement boundaries at top level. Braces in interpolation track nested blocks. Scanner work is O(n), token storage O(t), where n is source bytes and t token count.

## AST representation

ast.h defines AstKind, AstNode, AstLink, AstList, and AstArena. Nodes use kind-specific named fields (name/operator, first/second/third child, item list, flags, source span) rather than positional untyped arrays. Lists are singly linked arena-owned links with head/tail/count; append is O(1), traversal O(k). Text and literal payloads borrow source slices. AST nodes are immutable after construction. The arena is destroyed once the compilation unit is discarded.

Node families cover program/import/declarations (binding, function, class, interface, enum, alias), parameters and types; block/control/return/break/continue/expression statements; patterns; and literals, interpolation, collection/tuple, unary/binary/assignment/range, member/index/call, lambda, match, parallel, spawn/await/try, and kernel expressions. AST stores syntax only; name binding, types, and exhaustiveness belong to semantic analysis.

ast_arena_init(AstArena *, size_t); ast_new(AstArena *, AstKind, SourceSpan); ast_list_append(AstArena *, AstList *, AstNode *); ast_dump(FILE *, const AstNode *) are public. Construction is O(1) per node/link. Dump is O(nodes + text bytes), uses stable indentation and escaping, and returns false on I/O failure.

## Parser API and error handling

parser.h exposes parser_parse(AstArena *, const TokenArray *, Diagnostics *, AstNode **program). It produces a partial program AST when recoverable syntax errors occur; false means invalid API/allocation failure. Parser never performs name/type lookup. Each recursive-descent production validates expected delimiters. Pratt parsing uses a fixed binding-power table that matches GRAMMAR.md, including right-associative exponentiation/assignment, postfix call/member/index, and non-chainable comparisons/ranges.

Statement synchronization points are newline, semicolon, closing brace, and declaration starters. Recovery always consumes a token or returns at a caller-owned delimiter, preventing infinite loops. Parser tracks nested delimiter context so newlines are ignored in parentheses/brackets and before else. Nesting has a documented implementation limit; reaching it produces a diagnostic rather than exhausting the C stack.

Parsing is O(t) time and O(d + a) memory, where t is token count, d maximum syntactic nesting, and a AST node/link count. Recursive descent/Pratt is chosen for direct diagnostics and extensibility; a generated parser is rejected to keep the C11 MVP dependency-free and easier to debug.

## Phase 2 acceptance

Public headers are separate from implementations. Each source/lexer/AST/parser module has unit coverage. Golden cases compare canonical AST dumps for valid examples and expected diagnostic codes/locations for invalid examples. Tests cover UTF-8, CRLF, escapes/interpolation, precedence, associativity, trailing commas, newline rules, parser recovery, and bounded nesting. Every C translation unit compiles with C11 and -Wall -Wextra -Wpedantic without warnings.
