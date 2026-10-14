# Roadmap

Status: Phase 0 design, revision 2. Complete one phase at a time. Each phase leaves a buildable, reviewable repository. Compile C using cc -std=c11 -Wall -Wextra -Wpedantic with zero warnings. Every module has unit tests; language behavior has golden tests. Third-party code requires third_party placement and a license note.

## Phase 0 — Design

Deliver SPEC.md, GRAMMAR.md, IR.md, MEMORY.md, CONCURRENCY.md, and ROADMAP.md. Define canonical syntax, types/object model direction, IR/backend contract, GC/escape rules, and concurrency/GPU semantics. Acceptance: substantive consistent docs without placeholders and explicit phase gates.

## Phase 1 — Foundation

Create Makefile, test runner, unit harness, and src/common arena, vector, map, string/byte buffer, diagnostics, platform modules. Separate headers and implementation. Check overflow/alignment/failure. Acceptance: repeatable tests and warning-free C11 build.

## Phase 2 — Lexer and parser

Implement UTF-8 sources/spans, tokens, interpolation-aware lexer, AST/dump, recursive-descent declarations/statements, Pratt expressions. Add unit and valid/invalid golden tests. Acceptance: source diagnostics, trailing commas, nested interpolation, recovery without hangs.

## Phase 3 — Semantic analysis

Implement module/name resolution, signatures, types/inference, generics, unions/ADTs/pattern exhaustiveness, classes/interfaces, visibility, mutability, diagnostics, and conservative escape/effect analysis. Reject static errors before execution.

## Phase 4 — SSA IR and lowering

Implement typed IR, builder, lowering, verifier, source maps, pass framework and dump. Include map/reduce/parallel_for/kernel_launch/spawn/await in initial schema. Add constant folding, DCE, guarded inlining. Acceptance: verifier rejects malformed CFG/effects and every lowered module verifies.

## Phase 5 — Runtime and VM

Implement values/descriptors, strings/collections/ADT values, bootstrap managed heap, VM frames/dispatch, and IR-to-bytecode lowering. Add one-command run and REPL. Golden tests cover semantics, traps, output, Result, and sequential task fallback.

## Phase 6 — Full object model

Implement classes, promoted constructors, single inheritance, stateless interfaces, vtables, methods, visibility, reflection, virtual IR calls. Extend escape analysis for instances, closures, and method captures.

## Phase 7 — Full GC

Implement precise roots/stack maps, non-moving nursery bump allocator, old mark-sweep, remembered sets/barriers, promotion, stats, compiler arenas, temporary regions, and task/device roots. Stress collection and test lifetime/failure.

## Phase 8 — C backend

Emit C from verified IR, define root/runtime ABI, invoke configured cc, implement lang build source -o output. Create standalone native binaries without web-server dependency and support embedding through C ABI. Compare native golden results with VM.

## Phase 9 — Numerics and matrices

Specify fixed-width numbers, Decimal context, BigInt/BigFloat, Complex[T], Vector[T], Matrix[T], collection/shape/layout rules. Add scalar reference, SIMD dispatch, vectorization, reproducible benchmarks, differential tests, and numerical determinism policy.

## Phase 10 — GPU backend

Define GPU interface/capabilities, device ownership/transfers, kernel restrictions, CUDA/PTX lowering, and parallelize pass; add SPIR-V after semantics stabilize. Test unsupported diagnostics, synchronization/lifetime, and CPU/GPU behavior.

## Phase 11 — WASM backend

Emit WASM from verified IR for explicit browser/WASI profiles. Specify imports/runtime ABI. Test supported programs against VM and diagnose unsupported features.

## Phase 12 — Standard library and tooling

Deliver core/math/matrix/I/O/network libraries including http.server, JSON, formatter, LSP, package manager, reflection metadata, and CLI run/build/REPL; optional dev server is library-backed. Document standalone, embedded, and WASM workflows.

## NativeDB product goal

NativeDB is an embedded, single-node, matrix-first database library linked into
the Matra runtime. It is not a separate server and does not add a web-server
dependency. Its design constraints, format/query boundaries, zero-copy limits,
GC integration, and safety rules are in `NATIVEDB.md`.

NativeDB is delivered as a subsystem after its prerequisites, without changing
the language phase order above:

- Design gate: specify the `.ndb` format, C ABI, durability defaults, alignment,
  mapping lifetime, supported types, query DSL, and benchmark methodology.
- Storage foundation: portable mapped-file abstraction, aligned allocation,
  checksummed header/footer, bounds validation, and WAL/recovery tests.
- Matrix storage: tiled dense Float32/Float64 matrices, aligned tiles, and
  borrowed zero-copy views when mapping/layout/endianness permit.
- Columns and indexes: columnar table fields, validity bitmaps, encoding policy,
  and initially scoped scalar/vector indexes.
- Shared-IR queries: typed database operation families in SSA, with a rule
  planner and CPU reference execution; no separate query backend IR.
- Advanced capabilities: joins/aggregates/documents, optional GPU/UMA, adapters,
  and MVCC only after the corresponding correctness and portability gates.

NativeDB may begin format design alongside compiler work, but runtime linkage
waits for the runtime/VM and shared-IR contracts. Matrix GPU access waits for
the GPU capability, ownership, and synchronization contracts. Performance
targets from the product brief remain unaccepted until reproducible benchmarks
exist.

## Cross-phase gates

Update specs/tests before semantic changes. Backends consume shared verified IR only. Unsupported features diagnose. C remains warning-free. Every module has unit tests. Optimizations compare to unoptimized reference; performance claims include reproducible benchmarks. Native runtime stays independent of HTTP/server libraries.
