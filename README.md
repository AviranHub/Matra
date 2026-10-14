# Matra Language

Matra is a C11 language implementation project targeting 64-bit desktop and server systems. Its planned execution targets are an SSA-based VM, standalone native binaries, GPU kernels, and WASM. The language has no web-server runtime dependency; HTTP belongs in the standard library.

Matra source files use the `.mat` extension (for example, `examples/calculator.mat`).

The implementation roadmap is tracked in docs/ROADMAP.md. Language design is recorded in docs/SPEC.md, docs/GRAMMAR.md, docs/IR.md, docs/MEMORY.md, and docs/CONCURRENCY.md. NativeDB is an additional embedded, matrix-first database goal specified in docs/NATIVEDB.md; it is a planned subsystem, not implemented functionality.

## Build and test

With GCC and GNU Make installed, run make test. On Windows with MinGW, run mingw32-make test. The test runner is also available as bash tests/run_tests.sh.

Build artifacts are written to build/. The C sources are compiled as C11 with -Wall -Wextra -Wpedantic -Werror.
