# Semantic Analysis Design

Status: Phase 3 contract; implementation is partial.

## Pipeline and ownership

Semantic analysis consumes a parsed AST, records side-table types and escape
facts, and reports diagnostics without mutating syntax nodes. The intended
pipeline is declaration collection, lexical resolution, type checking/inference,
then escape/effect analysis. AST, tokens, and source outlive `SemaContext`;
context init/destroy own symbol and annotation arrays. Diagnostics own messages.

`sema.h` currently exposes `SemaContext`, `SemaType`, `SemaTypeKind`,
`SemaEscape`, `sema_analyze`, `sema_type_of`, `sema_escape_of`, and
`sema_type_name`. Public symbol/options records and context-owned structural
type descriptors remain planned.

## Current implementation

Types use static descriptors for Error, Unknown, Unit, Bool, Int64, Float64,
String, Char, Function, and Collection, plus context-owned nominal and
structural descriptors. Integer/float literals default to Int64/Float64;
collection literals infer the broad Collection type. Generic type applications
for a limited set of built-ins and declared nominal arity are represented, and
union annotations are flattened and deduplicated. Compatibility recursively
compares generic arguments and checks union inclusion. Generic function
inference is limited to direct parameter/result type variables; constraints,
nested unification, and canonical structural identity are not implemented.

Symbols retain borrowed token slices, declaration/value nodes, type,
mutability, and numeric scope depth in a growable flat array. Top-level function
names are collected before bodies. Bindings become visible after checking their
initializer. Nested blocks, parameters, and loop bindings use lexical scopes.
Lookup and duplicate checks are O(s) in the number of visible symbols; hash
tables and visibility/kind metadata remain future work.

Parser nodes distinguish class methods, expression-bodied functions, and
constructors with separate flags. Tuple elements use argument wrappers, empty
tuple syntax has Unit type, field initializers are checked, and common indexing,
numeric ranges, lambda bodies, kernels, comprehension bindings, and async-like
prefix expressions receive basic semantic traversal. Unannotated block
functions infer a result from explicit return statements when the declaration
has already been analyzed.

Implemented diagnostics cover duplicate bindings/functions, unknown names and
types, immutable writes, broad assignment/operator compatibility, Bool
conditions/guards, return and loop-control context, duplicate Bool/ADT cases,
non-exhaustive Bool/ADT matches, variant existence/payload arity, and
incompatible match-arm result types. Tuple/list destructuring binds names.
Class fields/method bodies receive partial checking; declared/promoted field and
method lookup is type-checked, method calls use declared signatures, and class
constructor arguments are checked against `init`. Public/private member access
is checked, and a class with a directly named interface base is checked for
required methods, parameter labels/types, and result types. Inheritance,
visibility contexts beyond same-class access, and generic interface
conformance are not implemented. Escape analysis remains conservative:
passing a value to any call marks it escaping; this is safe but limits future
stack-allocation opportunities.
Literal scalar/string/char/bool nodes are marked non-escaping; directly returned
or passed local initializer expressions are marked escaping. Other values
remain unknown. These escape facts do not authorize stack allocation.

Imports/module resolution, full function-type values and overload resolution,
inheritance, nested generic inference, type variables beyond direct generic
function parameters, effect
classification, and `@pure` verification are not implemented. Generic
applications accept broad collection literals because their element types are
not inferred; this is not sound element-type checking. Imports are explicitly
rejected because no module loader exists. Union compatibility supports
annotation compatibility. Match exhaustiveness requires a wildcard when a
union contains an open scalar type; exhaustive coverage for finite ADT/union
combinations and pattern payload typing remain incomplete.

## Intended completion

The completed phase will add canonical structural types and unification,
generic constraints, module loading, inheritance and complete class/interface
checks, full call signature/label checking, and sound escape/effect
facts. ADT/enum names and a limited form of exhaustiveness are already
implemented but require generic-aware payload typing and stronger coverage
analysis. Unknown calls and unsupported captures remain conservative. Semantic
analysis will not make allocation decisions; IR/GC phases consume verified
facts later.

## Complexity and acceptance

Current analysis is O(n*s) in AST node count `n` and visible symbol count `s`.
Allocation failure handling requires further hardening. Phase 3 is complete
only when module/name resolution, structural inference, generics, union types,
full ADT payload typing/exhaustiveness, class/interface/visibility checking,
full call checking, and sound escape/effect analysis have tests. C sources
compile with C11, `-Wall -Wextra -Wpedantic -Werror`.
