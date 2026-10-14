#!/usr/bin/env bash
set -eu

if command -v make >/dev/null 2>&1; then
    exec make test
fi
if command -v mingw32-make >/dev/null 2>&1; then
    exec mingw32-make test
fi
printf '%s\n' "error: GNU make was not found" >&2
exit 127
