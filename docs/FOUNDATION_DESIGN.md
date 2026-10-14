# Foundation Module Design

Status: Phase 1 API and implementation contract.

## Shared conventions

All public declarations live in matching headers under src/common; implementations live in C files. Public sizes/capacities and source positions use size_t. Fixed-width hashes and language-facing fixed-width integers use uint64_t/int64_t. No public API assumes long is 64 bits. Fallible allocation APIs return bool or a documented null pointer and leave the prior object valid on failure. All dynamic byte-size and capacity arithmetic is overflow-checked. Containers own their backing allocations; values copied into containers are byte copies, not destructed objects.

## Arena

Public surface in arena.h: Arena; arena_init(Arena *, size_t block_size); arena_alloc(Arena *, size_t size, size_t alignment); arena_calloc(Arena *, size_t count, size_t size, size_t alignment); arena_reset(Arena *); arena_destroy(Arena *).

Arena owns a singly linked chain of malloc-backed blocks. Each block records capacity and used byte count; payload starts at max_align_t alignment. Requests align the cursor to a power-of-two alignment no greater than the implementation's supported fundamental alignment. Oversized requests get a dedicated sufficiently large block. Zero-byte requests return a unique non-null allocation of at least one byte. Reset keeps the first reusable block and releases later blocks; destroy releases all blocks. Returned pointers are invalid after reset/destroy. No individual free or realloc is provided.

Allocation is O(1) when the current block has room and O(number of newly allocated blocks) otherwise; reset/destroy are O(number of blocks). Arena does not run destructors. This makes compilation-unit lifetimes cheap and explicit; clients must not put externally owned resources requiring cleanup into it.

## Vector

Public surface in vec.h: Vec; vec_init(Vec *, size_t element_size); vec_reserve(Vec *, size_t capacity); vec_push(Vec *, const void *element); vec_resize(Vec *, size_t length); vec_at(Vec *, size_t index); vec_at_const(const Vec *, size_t index); vec_clear(Vec *); vec_destroy(Vec *).

Vec stores fixed-size elements in a contiguous byte buffer with length and capacity. Push and growth copy element bytes; push supports a source element already stored in the same vector. Resize growth zero-initializes new bytes. Capacity grows geometrically (initial capacity 8, then doubling) with checked multiplication. A failed reserve/push/resize leaves length and contents unchanged. Accessors return NULL for out-of-range indices. No element callbacks or ownership semantics are implied.

Index access is O(1); push is amortized O(1), worst-case O(n); reserve is O(n) when it relocates; clear is O(1); destroy is O(1).

## Map

Public surface in map.h: Map; map_init(Map *, size_t value_size); map_put(Map *, const void *key, size_t key_length, const void *value, bool *replaced); map_get(Map *, const void *key, size_t key_length); map_get_const(const Map *, const void *key, size_t key_length); map_remove(Map *, const void *key, size_t key_length); map_clear(Map *); map_destroy(Map *).

Map is a byte-key, fixed-size byte-value table with owned key copies, open addressing, linear probing, and tombstones. Hashing is 64-bit FNV-1a; hash equality is followed by length and byte comparison. Capacity is a power of two and grows before the live-plus-tombstone load reaches two thirds. Rehashing preserves entries. Value slots use a stride aligned for max_align_t; the declared value_size is fixed for a map's lifetime. A put copies the value bytes and accepts a source value already stored in the map. A get returns a pointer valid until a mutating operation that rehashes/removes that entry, or map destruction. Keys may contain NUL bytes. Zero-length keys are valid.

Lookup/insert/remove are expected O(1), worst-case O(n). Rehash is O(n). A failed insertion/rehash leaves all existing entries and values intact. Map has no iteration-order guarantee. Clients needing deterministic order keep a separate Vec of keys.

## String buffer

Public surface in str.h: StrView { const char *data; size_t length; }; StrBuf; str_init(StrBuf *); str_append(StrBuf *, const char *bytes, size_t length); str_append_cstr(StrBuf *, const char *text); str_clear(StrBuf *); str_view(const StrBuf *); str_cstr(const StrBuf *); str_equal(StrView, StrView); str_destroy(StrBuf *).

StrBuf owns a growable byte buffer. For an initialized buffer, data is NUL-terminated after every successful operation and length excludes the terminator. Embedded NUL bytes are supported by length-aware operations. The buffer performs no UTF-8 validation or normalization. Growth is geometric and overflow-checked; a failed append leaves prior bytes and length unchanged. str_cstr returns a valid empty C string for an initialized empty buffer.

Append is O(k) for k appended bytes, amortized O(k), with O(n+k) worst-case relocation. Equality is O(min(n,m)) and checks lengths first.

## Diagnostics

Public surface in error.h: SourceSpan { size_t start_byte, end_byte, line, column; }; DiagSeverity; DiagCode; Diagnostic; Diagnostics; diagnostics_init(Diagnostics *); diagnostics_add(Diagnostics *, DiagSeverity, DiagCode, const char *file, SourceSpan, const char *message, const char *hint); diagnostics_print(const Diagnostics *, FILE *); diagnostics_clear(Diagnostics *); diagnostics_destroy(Diagnostics *).

Diagnostics owns copies of file, message, and optional hint strings, and stores records in a growable array. Positions are one-based for line/column; byte spans are half-open. Error codes are stable enum values for common foundation failures; later compiler modules may reserve ranges. Rendering is human-readable: file:line:column: severity [code]: message, followed by a hint line when present. Allocation failure while recording a diagnostic is returned as false and does not corrupt earlier diagnostics. No global mutable error state is used.

Adding is O(message length + file length + hint length), amortized O(1) record growth; printing/clearing is O(total diagnostic text).

## Platform

Public surface in platform.h: PlatformInfo; platform_info(void); platform_path_is_absolute(const char *path); platform_path_separator(void).

PlatformInfo reports static OS and architecture names, sizeof(void *) * CHAR_BIT, and native endianness. OS/architecture selection uses compiler predefined macros; unknown targets report "unknown" instead of failing compilation. Absolute-path detection recognizes native roots (drive-root and UNC on Windows; leading slash on POSIX). Path separator reports the host convention. These functions do not perform I/O or allocate.

Detection is O(1); path classification is O(path length). Process spawning, filesystem traversal, and executable discovery are deferred to CLI/backend phases rather than added to this small portability layer.

## Dependency direction

arena and platform depend only on the C standard library. vec and str depend on checked common size helpers local to their implementations or a future internal header. map owns its allocation and does not depend on str. error uses its own owned strings and does not depend on vec/str. Tests use public headers only.
