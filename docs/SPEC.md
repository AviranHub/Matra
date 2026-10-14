# Language Specification

Status: Phase 0 design, revision 2.

## Goals and conformance

The language is general-purpose, statically typed, object-oriented, and uses local type inference. The implementation is C11 for 64-bit desktop/server systems. It ships an interpreter, native compiler, and WASM frontend; C embedding is supported. HTTP is a standard-library package, never a runtime dependency.

The pipeline is source → lexer → parser → AST → resolver/type checker → typed SSA IR → analysis/optimization → VM, C/native, GPU, or WASM backend. Every backend consumes the same verified IR; no backend reads AST or performs type inference. Observable evaluation, error, and numeric rules agree across backends.

## Source and diagnostics

Source is UTF-8; identifiers are ASCII. LF and CRLF are accepted. A BOM is valid only at offset zero. Internally, source spans are half-open byte ranges. Diagnostics show file, one-based line and column, message, and a useful hint. Columns count Unicode scalars for valid text; malformed UTF-8 is reported at its byte.

Imports are compile-time modules. Runtime include/require is rejected. Module resolution, initialization cycles, and package version rules must be specified before modules ship. A dot separates module path components; :: may qualify members. Backslash namespaces and a global keyword are excluded.

## Lexical and syntactic conventions

Identifiers match [A-Za-z_][A-Za-z0-9_]* and are case-sensitive. Keywords include let, var, fn, class, init, interface, type, enum, match, if, else, for, in, while, return, break, continue, import, as, public, private, protected, true, false, and, or, not, parallel, spawn, await, try, self.

Comments begin with // and run to line end. Integer and floating tokens have no sign; signs are unary operators. Strings use double quotes, escapes for quote, backslash, newline, carriage return, tab, and NUL, and interpolation using braces. Double braces represent literal braces. Interpolations are expressions evaluated left-to-right once. Trailing commas are accepted in every delimited list. Semicolons are optional: a top-level newline terminates a complete statement unless the next token continues its expression; newlines inside parentheses and brackets are insignificant. A closing block brace and EOF terminate the final statement implicitly.

## Canonical syntax

Bindings use let name = expression and var name = expression; annotations use let name: Type = expression. let prevents rebinding; var permits rebinding. Object/view mutation is governed by types and ownership rules, not binding mutability alone.

Functions use fn name[T](x: T) -> T { ... } or expression body fn square(x: Int) -> Int = x * x. Generics use square brackets. Lambdas have one syntax: (x: Int) => x * 2. Parameter types may be inferred only from an expected callable type.

Classes use class Name [: Parent] { ... }; constructors are init and there is no new keyword. Constructor property promotion uses init(self.x: Float64, self.y: Float64) {}. Methods refer to receiver self; method declarations omit an explicit self parameter, but receiver is in callable type/dispatch ABI. Single inheritance only. Interfaces are stateless method contracts; implementation and default-method rules are completed in the object-model phase.

ADTs use type Option[T] = Some(T) | None and type Result[T,E] = Ok(T) | Err(E). Enums have named cases and may have methods. match is an expression without fall-through or break. Patterns include cases, literals, wildcard, tuples/destructuring, alternatives, and integer ranges. Exhaustiveness and unreachable arms are checked statically.

Control flow includes if condition { ... } else { ... }, while condition { ... }, and for item in iterable { ... }. Conditions do not require parentheses. Ranges are start..end (half-open) and start..=end (closed), with checked endpoint overflow. break and continue target the innermost loop.

Operators: + - * / % **, comparisons, word operators and/or/not, assignment = and compound += -= *= /= %= **=, matrix multiply @, and ranges .. and ..=. and/or short-circuit. Assignment requires a mutable place and evaluates its left place once before the right side. @ is typed matrix/vector multiplication, never implicit elementwise multiply.

Calls support positional and named arguments using name: expression. No duplicate arguments; positional arguments cannot follow named ones. Evaluation follows written order. Any future defaults evaluate at the call site in declaration order. Generic type application uses brackets, e.g. Vector[Float64](...). Parsing distinguishes type application from indexing using syntax plus semantic resolution.

## Types and static semantics

Primitive types: Bool, String, Char, Int8/16/32/64, UInt8/16/32/64, Float32/64, Decimal, BigInt, and BigFloat. Int aliases Int64; Float aliases Float64. Complex[T] is numeric over supported components. Collections include List[T], Vector[T], Matrix[T], Map[K,V], Set[T], Tuple, Option[T], Result[T,E], and Task[T,E]. Task is an affine handle and cannot be duplicated or discarded without joining its scope.

No null or implicit type juggling. Absence uses Option; errors use Result; checked exceptions are rejected. Equality is strict. Union signatures such as Int | Float64 | ParseError are normalized, deduplicated sum types with explicit refinement, not coercion. Numeric conversions are explicit except contextually typed literals whose exact value fits the expected type. Numeric precision and rounding follow the contracts below; allocation failure is reported as a runtime error.

Int8/16/32/64 and UInt8/16/32/64 have exact widths; Int/UInt default to 64 bits. Integer operations trap on overflow unless explicitly named wrapping or saturating operations are used. Float32/64 use IEEE 754. Decimal is base-10 with a context carried by the operation; the initial standard context is 34 significant decimal digits, round-half-even, with exact addition/multiplication when representable. BigInt is arbitrary precision subject to allocation limits. BigFloat has per-value precision, defaulting to 256 binary bits, and round-to-nearest ties-to-even. Context changes are explicit and scoped; they never silently alter unrelated tasks.

Char is one Unicode scalar value; String is immutable UTF-8. List is a resizable sequence; Vector is a fixed-length contiguous numeric sequence; Matrix is a fixed-shape contiguous row-major numeric tensor of rank two. Matrix dimensions and allocation products are checked. A flat list literal infers Vector[T]; a rectangular nested list infers Matrix[T]. Ragged nested literals are errors. An expected List[T] context selects List instead. Empty literals require an expected collection type. Map/Set/Tuple have distinct types and do not alias list semantics.

Tuple values use parenthesized comma-separated expressions. Map/Set construction uses standard-library constructors. Matrix multiplication uses @ or the explicit matrix.mul call; * is elementwise/scalar arithmetic only where the operand type defines it. Matrix shapes must satisfy inner-dimension equality; result shape is rows(left) by columns(right). Vector/matrix storage is contiguous row-major, with the last index varying fastest.

Type parameters use square brackets and are invariant by default. Variance is explicit. Generic constraints are compile-time bounds, not runtime coercions. Inference is bidirectional: expression types flow outward and expected types into literals, lambdas, and generic calls. Empty collections or unconstrained lambda parameters require context. Recursive functions declare result types. Union values carry a runtime case tag; match, type tests, and documented refinement operations narrow the static type. Overlapping union members are canonicalized and duplicate members removed.

let/var controls rebinding. public/private/protected independently control access. readonly is not a separate modifier; immutable bindings and views are expressed with let and types. Object-model phase defines access and initialization guarantees.

## Evaluation, errors, and containers

Operands, call arguments, collection elements, interpolation segments, and constructor arguments evaluate left-to-right. Only and/or short-circuit. not is unary. There is no truthiness.

Integers are fixed-width two's complement. Overflow traps unless an operation explicitly declares wrapping/saturating behavior; division by zero traps. Float uses IEEE 754. Decimal has base-10 arithmetic and explicit context; BigInt/BigFloat have documented precision/resource limits; Complex follows component types. Their exact contracts precede the numeric implementation phase.

try expression unwraps a successful Result or propagates its error from the current function when its error type is assignable to the enclosing Result error type or union. It does not catch traps. No checked exceptions or error suppression. Runtime traps report source location.

Bounds and shape errors are checked. List/Vector/Matrix iterate in row-major/index order. Map/Set order is unspecified unless an ordered variant is used. Concurrent mutation requires specified ownership/synchronization; ordinary data races are forbidden.

## Concurrency, GPU, and attributes

spawn expression creates Task[T,E], where T is the task result and E its typed failure set; await yields T or propagates E into the enclosing Result/union type. Cancellation, failure, scope lifetime, and race rules are in CONCURRENCY.md. await synchronizes. Scheduler need not allocate one OS thread per task.

parallel requests parallel execution for supported pure work; parallel sum(x for x in xs) is a reduction with explicit policy. kernel(domain) { x => ... } defines GPU-mappable computation. Attributes include @vectorize(width: 8), @inline, @pure, and @gpu(kernel: "matmul", block: (16,16)); schemas and legality are checked. Hints never permit semantic changes. Unsupported targets diagnose or use valid fallback.

matrix.mul may select CPU SIMD or GPU based on capability, transfer cost, and size. parallel requests parallel execution. Numerical result follows declared reduction policy; dispatch thresholds do not change semantics.

## Reflection, tooling, and deployment

typeOf(value) and metadata for fields/methods expose declared public metadata subject to visibility; reflection cannot mutate private layout or bypass typing. Deliverables include REPL, one-command run, formatter, LSP, human-readable diagnostics, and package tooling. Formatter preserves semantics; LSP shares compiler spans/codes.

Rejected: loose equality, type juggling, $ sigils, one array type for list/map/tuple, runtime include/require, global declarations, error suppression, magic methods, variable variables, backslash namespaces, new, multiple inheritance, null, implicit ??, and checked exceptions. Use distinct collections, compile-time imports, Option/Result, and typed calls.

Native builds create standalone binaries; WASM uses a documented browser/WASI profile; C embedding uses a documented ABI. HTTP server is a library (http.server), not runtime/compiler behavior. CGI/FastCGI can be library integrations but is not default. lang build server.lang -o server creates standalone output.

## Embedded data store goal

Matra additionally targets NativeDB, an embedded, single-node, matrix-first
database linked into the runtime. It stores tiled matrices, columnar tables,
scalars, text/bytes, and nested documents; queries compile to the same typed
SSA IR used by the language. NativeDB is not a separate server or a full-SQL
replacement. Mapped views must retain their mapping owner and participate in
GC roots and escape analysis. Zero-copy applies only when the stored encoding
is directly compatible with the requested host view; other encodings require
an explicit decode path. See `NATIVEDB.md` and the NativeDB milestones in
`ROADMAP.md`. This is a planned product goal, not current functionality.
