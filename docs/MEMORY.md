# Memory Model

Status: Phase 0 design, revision 2.

## Ownership and object model

Runtime combines precise managed objects, escape-analysis stack placement, generational GC, compiler arenas, temporary numeric regions, and explicit device allocation. It targets 64-bit desktop/server. Byte sizes/capacities use size_t; language fixed-width integers use int64_t/uint64_t; uintptr_t is for checked pointer operations only. Never assume long is 64-bit.

Managed references are opaque. Objects have private headers and descriptors for layout/tracing. Strings are immutable UTF-8. Collections and user objects trace contained references. Object identity is stable. Moving collection is excluded until an ABI guarantees reference updates.

## Roots and safepoints

Roots include VM stacks/frames/locals, globals, constants, native handles, live temporaries, and runtime roots. VM enumerates typed slots. Native code uses precise stack maps or explicit shadow-frame handles; conservative scanning is not the contract. Finalize native root ABI before the C backend ships.

Allocation-capable calls and gc_poll are safepoints. Every live managed reference across one must be rooted. Descriptors trace initialized fields only. Constructors root inputs, initialize fields safely, and publish complete objects only. Collection at any safepoint does not change observable behavior.

## Generational collector

Young generation uses aligned nursery pages and checked bump allocation; validate size arithmetic, alignment, and capacity before moving the cursor. Large objects may go directly old. Old generation uses mark-sweep with implementation-selected free-list/coalescing policy.

References remain stable, so nursery is non-moving. Reclaim pages/segments and free lists; never copy objects and leave stale references. Minor collection stops mutators, scans roots and remembered old objects, traces young reachability, reclaims dead storage, promotes survivors by age/space policy, updates remembered metadata, and resumes. A write barrier records every old-to-young reference store using a card table or remembered-object set.

Major collection stops mutators, traces reachable objects, sweeps unreachable old objects, and reclaims unreachable young storage. Mark bits reset or use epochs. Initial GC is single-mutator stop-the-world; parallel mutators need a separate coordination design. Collector metadata failure must fail safely.

VM may bootstrap with non-moving mark-sweep. Full GC adds nursery/old generations without changing identity/root contracts.

## Escape analysis and allocation regions

Escape analysis precedes placement. Stack placement requires proof that the object cannot outlive activation, be returned, stored in longer-lived memory, captured, sent to another task/device, or retained by unknown code. Unknown calls are conservative. Closures and spawned tasks extend lifetimes and are analyzed. Uncertain allocation uses managed heap.

Stack objects containing references need stack-map/descriptor treatment and cease being roots at frame exit. Return requires safe promotion or copy.

Compiler tokens, AST, diagnostics, type tables, and IR scratch use compilation/phase arenas. Allocation is aligned and overflow-checked; reset invalidates all pointers. Retained data is promoted/copied.

Temporary matrix/vector work may use per-thread/operation regions with explicit owner/lifetime. Values cannot escape without copy/promotion. Region references into managed heap remain GC-visible. GPU buffers have separate ownership/transfers/synchronization; host GC never frees implicitly. Do not release a buffer while a kernel uses it.

NativeDB mappings are external memory, not GC heap. A GC-managed database
handle owns the mapping lifetime; borrowed matrix/table/document views retain
that handle (directly or through a rooted owner) so unmapping cannot invalidate
live views. Escape analysis must treat views as references to the mapping:
views that escape their owning scope retain the handle, and unknown calls are
conservative. Writable mapped access follows the database transaction/WAL
contract. Zero-copy is available only when the on-disk encoding and host layout
are directly compatible; decoded values use explicit owned/region storage.

## Failure and invariants

Check size multiplication, capacity growth, alignment, and addition for size_t overflow. Allocation failure reports source-located runtime diagnostics and a hint when possible. Never publish partially initialized objects.

Every live managed reference is rooted/reachable at safepoints; descriptors trace initialized references; old-to-young stores are remembered; stack/arena/region values do not outlive owners; task/device handoff preserves roots and ownership; GC preserves identity and values. Stress collection at each allocation and test roots, barriers, promotion, sweep, failure, escaped closures, and device lifetime.

Concurrent/moving GC, finalizers, weak refs, pinning, and user-controlled regions need separate contracts before implementation.
