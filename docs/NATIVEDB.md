# NativeDB Goals and Architecture

Status: Product goal and architectural direction; not yet implemented.

## Product boundary

NativeDB is the embedded, single-node native data store for Matra. It is a C11
library linked into the runtime, not a separate service. It is matrix-first:
matrix layout, matrix query execution, and matrix/accelerator interoperability
are primary design constraints. It also stores columnar tables and scalar,
text, byte, temporal, and nested-document values. It is not a distributed
database, a full-SQL replacement, or a multi-file ACID system. The initial
write model is one writer per file; multi-writer MVCC is deferred. HTTP remains
a standard-library concern, unrelated to the database runtime.

## Core goals

- Provide a versioned `.ndb` format with explicit little-endian fields,
  validated offsets/lengths, checksums, and checked arithmetic.
- Map files read-only/read-write on supported platforms (Unix `mmap`, Windows
  file mappings). Return borrowed views into mapped bytes when the requested
  representation is directly addressable. Compressed, endian-converted,
  encoded, or otherwise non-native data may require decoding; that path must be
  explicit and must not be advertised as zero-copy.
- Store tables column-wise and dense matrices in cache-friendly tiles. Tile
  order is row-major over tiles and row-major within each tile. Tile dimensions
  are format metadata, not a compatibility assumption. Numeric buffers target
  at least 32-byte alignment; 64-byte alignment is preferred. Mapping offsets
  must also satisfy the platform's mapping granularity. A 4 KiB alignment goal
  for GPU-mappable regions is separate from SIMD alignment.
- Keep matrix/vector data contiguous within tiles and expose views without
  copying where layout permits. Support scalar columns, validity bitmaps,
  strings/bytes, and nested documents without imposing work on matrix scans.
- Use the language's `Matrix[T]`, `Vector[T]`, scalar, string, and document
  type contracts. NativeDB views refer to mapped/non-GC storage; handles and
  views must keep the mapping alive and be visible to GC/root and escape
  analyses. Temporary query results use explicit arenas/regions.
- Compile query expressions to Matra's shared typed SSA IR. The database does
  not define a second backend IR and no backend reads query AST. Query
  optimization reuses verified constant folding, DCE, vectorization,
  parallelization, and escape/effect analyses.
- Provide a query DSL first, not full SQL. Matrix operations (matmul, element
  operations, transpose, reductions, broadcasting, vector similarity) take
  priority; table scan/filter/project/aggregate/join and document access are
  additional capabilities.
- Plan for checksummed WAL, configurable durability, checkpointing, and
  recovery from the first persistent format. The default durability policy
  must be explicitly specified before implementation; strict per-commit sync
  is available for callers that request it. Transactions are single-file.
- Expose a stable C ABI for embedding and future adapters. Optional compression,
  numerical libraries, and GPU drivers stay optional, documented dependencies.

## On-disk regions and logical views

The format design reserves versioned header/footer metadata, schema, indexes,
data, validity/null metadata, WAL/checkpoint metadata, statistics, and
checksums. Readers validate every region before dereferencing mapped offsets.
Schema changes are versioned; incompatible changes require an explicit
migration policy. Dense matrices use tiled row/column metadata with checked
dimension products. Sparse matrix/vector encodings are separate types and
never masquerade as dense zero-copy buffers.

One stored object may be exposed as document, table, matrix, vector, or
time-series views when the schema/layout supports that interpretation. This is
logical composability, not a promise that arbitrary conversions are free.
Compression and dictionary encoding are block/column choices and must preserve
an uncompressed/direct-view option for latency-sensitive matrix data.

## Query and IR contract

The planned logical/physical query planner lowers to the same SSA IR as user
code. The IR schema must reserve typed database operations for open/close,
load/store/scan, filter/project/join/group/aggregate/order/limit, matrix tile
load/store, matrix multiply/transpose/axis reduction/broadcast, vector search,
logical views, device capability queries, shared allocation, and
synchronization. These operations carry memory, I/O, allocation,
synchronization, and device-transfer effects. The optimizer may not remove or
reorder observable database effects. GPU execution is capability-gated with
explicit transfer and sync; UMA/shared-memory access is an optional platform
capability, never a universal assumption. CPU/SIMD remains the correctness
fallback.

## Durability, concurrency, and safety

Initial concurrency is multiple readers and one writer per file, protected by
cross-process file locking. WAL records are checksummed, ordered, and replayed
only when committed; recovery validates the header and data regions before
exposing the database. Checkpoint thresholds and fsync policy are configurable
and documented. MVCC/snapshot isolation are later work, not implied by mmap.

All offsets, lengths, dimensions, element counts, and alignment calculations
use checked `uint64_t`/`size_t` arithmetic. File data is untrusted input:
validate magic, version, endianness, checksums, bounds, schema references, and
tile indexes before use. Mapping lifetime, write permissions, crash semantics,
thread safety, and API error context are part of every public API contract.

## Deferred choices and acceptance gates

Tile-size defaults, compression codecs, checksum algorithm, index formats,
query syntax, WAL fsync defaults, UMA/device APIs, and C ABI versioning require
benchmarks or platform-specific design before implementation. Performance
targets are hypotheses, not promises: every published result requires a
reproducible benchmark and a named baseline. Tests must include format
corruption, crash/recovery, mapping lifetime, concurrency, differential query
results, and Linux/macOS/Windows coverage. Third-party code is optional and
must retain its license notice.

NativeDB delivery milestones and dependency ordering are recorded in
`ROADMAP.md`. No NativeDB subsystem is considered implemented merely because
its IR opcodes or type names have been reserved.
