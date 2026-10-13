#!/usr/bin/env bash
# Coverage-guided libFuzzer runs over the portable RF/application parsers.
#   tools/fuzz.sh [seconds per target] [target...]   (default 60 s, all targets)
# Needs clang with libFuzzer (CLANGXX=/path/to/clang++ to override). New
# coverage is merged into fuzz/corpus/<target>; tools/test.sh replays that
# corpus with g++. Crashes land in .pio/fuzz/<target>/ with a replay command.
set -euo pipefail
firmware_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
clangxx="${CLANGXX:-clang++}"
seconds="${1:-60}"; shift || true
targets=("$@"); [ "${#targets[@]}" -gt 0 ] || targets=(protocol link app_protocol)
if ! command -v "$clangxx" >/dev/null; then
  echo "clang++ not found; install clang (with compiler-rt) or set CLANGXX" >&2; exit 1
fi
declare -A sources=(
  [protocol]="src/Protocol.cpp"
  [link]="src/Protocol.cpp src/Link.cpp src/AppProtocol.cpp"
  [app_protocol]="src/Protocol.cpp src/AppProtocol.cpp"
)
for target in "${targets[@]}"; do
  [ -n "${sources[$target]:-}" ] || { echo "unknown target $target" >&2; exit 1; }
  work="$firmware_dir/.pio/fuzz/$target"; corpus="$firmware_dir/fuzz/corpus/$target"
  mkdir -p "$work/corpus" "$corpus"
  srcs=(); for s in ${sources[$target]}; do srcs+=("$firmware_dir/$s"); done
  "$clangxx" -std=c++11 -g -O1 -Wall -Wextra -Werror \
    -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined \
    -I "$firmware_dir/include" "${srcs[@]}" "$firmware_dir/fuzz/${target}_fuzz.cpp" -o "$work/fuzzer"
  echo "== $target: ${seconds}s"
  # LeakSanitizer cannot run under ptrace-based sandboxes; nothing here allocates long-lived memory.
  ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$work/fuzzer" -max_total_time="$seconds" \
    -print_final_stats=1 -artifact_prefix="$work/" "$work/corpus" "$corpus"
  ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$work/fuzzer" -merge=1 "$corpus" "$work/corpus" 2>&1 |
    grep -E 'MERGE-OUTER: [0-9]+ new files' || true
done
