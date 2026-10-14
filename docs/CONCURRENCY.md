# Concurrency and GPU Execution

Status: Phase 0 design, revision 1.

## Goals and constructs

Concurrency is explicit in syntax and typed IR: fibers/tasks, data-parallel maps/reductions/loops, and GPU kernels. Runtime scheduling is not observable except through synchronization, results/failures, I/O order, and relaxed floating-point policy.

Core forms are spawn expression, await task, parallel expression, parallel_for, parallel sum(x for x in xs), and kernel(domain) { x => ... }. IR includes spawn, await, map, reduce, parallel_for, and kernel_launch from its first schema. Sequential VM fallback is allowed if semantics are preserved.

## Tasks and fibers

spawn evaluates captures/arguments at the spawn point, creates Task[T,E], and schedules it. T is the result type and E is the statically inferred error set. await suspends until completion and yields T or propagates E into the enclosing Result/union return type. Completion happens-before continuation. Task handles are single-consumption to prevent silently losing failure.

Tasks are structured: the lexical scope that spawns a task joins outstanding children on exit and propagates unhandled failure. Detached tasks require an explicit library API. Cancellation is cooperative at safepoints. Captured immutable Send-safe values may be shared. Mutable values require ownership transfer or synchronization. The initial implementation may conservatively reject mutable/unsendable captures.

Tasks do not imply one OS thread each. VM may schedule cooperatively; native runtime may use a worker pool. Await is the specified synchronization edge.

## Parallel work and reductions

Parallel map/for requires independent iterations or explicit reduction/atomic operations. Compiler proves independence or requires a safe API; parallel is not permission to race. If not provable/supported, diagnose or execute sequentially.

Reduction declares operation, identity, domain, and order policy. Integer operations remain checked unless explicitly configured otherwise. Floating reductions default deterministic; relaxed reassociation requires opt-in and documents rounding bounds. Empty input uses declared identity or is rejected.

## GPU kernels and data

Kernel bodies are statically constrained to typed supported operations, indexing, pure computation, and explicit reductions. Host I/O, arbitrary allocation, virtual dispatch, unsupported recursion, and blocking are prohibited unless a target profile explicitly allows them. Captures/parameters define transfer and layout.

Host/device memory is distinct. Allocation, transfer, launch, completion, synchronization, and copy-back are explicit effects. Buffers may remain resident across kernels when ownership is known. Reading results or awaiting establishes synchronization. Free/reuse waits for dependent kernels.

Backend selection considers capabilities, data size, transfer cost, and device availability. matrix.mul may choose CPU SIMD/GPU; parallel requests parallel scheduling. Unsupported GPU operations diagnose or use valid CPU fallback. CUDA/PTX is first target; SPIR-V follows on shared IR.

## Memory, effects, and failure

GC traces task stacks, queued closures, suspended fibers, and transfer roots. Scheduler registers captures before the spawning frame can release them. Stop-the-world GC coordinates workers at safepoints. Device buffers remain separately owned.

Ordinary data races are forbidden; statically apparent races are rejected. Shared mutable state uses explicit synchronization APIs. Atomics default to sequential consistency until weaker orders are specified. IR effects include scheduling, sync, memory, I/O, and transfers. Optimizers cannot reorder across effect dependencies. @pure is checked against inferred effects.

Task failures propagate at await or structured scope exit as typed errors. Runtime traps remain traps with source locations. Cancellation differs from failure. Resource cleanup/defer semantics must be defined before resource-owning async code ships.

## Portability

VM can execute data-parallel regions sequentially as reference behavior. Native may use a work-stealing pool. GPU profile and numeric policy are explicit. Nondeterministic task order cannot be observed without synchronization.
