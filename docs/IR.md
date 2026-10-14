# Intermediate Representation

Status: Phase 0 design, revision 2.

## Goals and invariants

Typed SSA IR is the sole contract for VM, C/native, GPU (CUDA/PTX first, SPIR-V later), and WASM. Backends receive verified IR plus target layout/capabilities; they never inspect AST or infer types.

Values are immutable SSA definitions. Every block has typed parameters and exactly one terminator. Block parameters implement phi semantics; each incoming edge supplies parameters in order. Uses are dominated by definitions or are block parameters. Types, traps, effects, synchronization, and source locations are explicit. Parallel and async operations exist in the initial opcode schema even before source lowering emits them.

## Module and types

A module contains types, constants, globals, and functions. Functions contain signature, linkage, optional generic metadata, ordered blocks, and source name. Blocks contain ID, parameters, instructions, and terminator. IDs are module-local, independent of AST pointers.

Types include Bool/i1, fixed-width signed and unsigned integers, f32/f64, Decimal and arbitrary-precision handles, Char, String, nil/unit, tuple, list, map, set, vector, matrix, function, nominal class/interface, ADT/enum, Option, Result, and normalized union. Layout is target-specific; language Int64 remains exactly 64 bits. Generic functions retain constraints or are monomorphized in an explicit pass.

## Operation families

Constants/state: const, copy, global_load/store, field_load/store, element_load/store, map_get/set, tuple_extract, variant_make/test/extract, type metadata.

Arithmetic: typed integer add/sub/mul/div/rem/neg/power with checked/wrapping mode; float arithmetic; Decimal/BigInt/BigFloat library operations; complex operations; compare and strict equality. Each operation states overflow/rounding behavior. Traps use explicit exceptional edges or checked results; backends never silently wrap checked operations.

Calls/runtime: direct/indirect/virtual call, closure creation/call, alloc_object, alloc_collection, string_concat/interpolate, print, type_of, gc_poll, and Result propagation. Calls include signature, effects, and argument order. Named arguments are resolved by frontend.

Database integration reserves an IR family for NativeDB operations: database
open/close, load/store/scan, filter/project/join/group/aggregate/order/limit,
matrix tile load/store, matmul/transpose/axis reduction/broadcast, vector
search, logical views, device queries, shared allocation, and synchronization.
These remain effectful typed IR operations (I/O, memory, allocation, sync, and
device transfer as applicable); a query planner lowers into this same IR, never
a database-specific backend IR. Exact opcode signatures are finalized with
the query/type/runtime ABI, not guessed in this design reservation.

Terminators: jump, branch, switch, return, unreachable, trap, invoke. Branch consumes Bool. Switch represents checked tagged cases/ranges. Invoke has normal and error successors for typed propagation. Trap carries diagnostic and source metadata.

## Parallel and async operations

The initial schema includes:

- map: typed elementwise transform over an explicit domain.
- reduce: typed reduction with identity, ordering, and reassociation policy.
- parallel_for: domain, induction type, body region, captures, and memory effects.
- kernel_launch: typed kernel, grid/block, arguments, device, transfers, synchronization, and result policy.
- spawn: schedule typed task and return a task handle with lifetime/effect metadata.
- await: observe task result and establish synchronization and failure edge.

Regions are structured, typed IR, not opaque text. Captures are explicit. Verifier checks signatures, types, captures, effects, and termination. VM may execute data-parallel regions sequentially as a correctness fallback. GPU lowering diagnoses unsupported operations or falls back without semantic change. Spawn/await preserve failure propagation and happens-before edges.

Reduction order is explicit. Deterministic mode preserves order; relaxed mode permits only documented reassociation and is required before parallel floating reductions change rounding. Transfers and synchronization are visible effects. Kernel launch cannot be an untracked host call.

## Effects, lowering, and passes

Effects include pure, may-trap, allocation, memory read/write, I/O, scheduling, synchronization, and device transfer. Effect tokens or memory SSA order observable operations. DCE removes unused pure values only. Unknown calls conservatively may retain references and mutate state unless purity is verified.

Allocations carry placement hints (heap, stack candidate, arena/region, device) and safepoints. Escape analysis determines safe placement. Lowering creates CFG for branches/loops, branches for short-circuit logic, and decision CFG for statically exhaustive match. Closures have explicit captures. Generic lowering policy is explicit and backend-independent.

Passes declare preserved/invalidated analyses and verify output. Planned passes: escape analysis, guarded inlining, DCE, constant folding, vectorization, parallelization, kernel formation. Preserve effects, traps, source maps, layout, and synchronization. Canonicalization converges; unoptimized mode remains a reference.

## Verification and backends

Verifier rejects duplicate definitions, missing terminators, invalid targets/arguments, type mismatch, non-dominating use, malformed regions, task/effect errors, illegal device captures, and broken memory tokens. Verify after lowering and transforms; backend entry requires verified IR.

C emission uses fixed-width types and never assumes long is 64-bit. Native output links only the small language runtime. WASM uses a documented profile. GPU maps operations to capabilities and explicit transfers. VM bytecode lowers from IR only. Backends preserve evaluation order, traps, bounds/shape checks, Result behavior, synchronization, I/O, and source locations.
