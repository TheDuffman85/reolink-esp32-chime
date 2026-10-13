#!/usr/bin/env bash
set -euo pipefail
firmware_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python3 "$firmware_dir/test/version_test.py"
test_binary="$(mktemp /tmp/reolink-protocol-test.XXXXXX)"
link_binary="$(mktemp /tmp/reolink-link-test.XXXXXX)"
app_binary="$(mktemp /tmp/reolink-app-test.XXXXXX)"
identity_binary="$(mktemp /tmp/reolink-identity-test.XXXXXX)"
trap 'rm -f -- "$test_binary" "$link_binary" "$app_binary" "$identity_binary"' EXIT
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/src/Protocol.cpp" \
  "$firmware_dir/test/protocol_test.cpp" -o "$test_binary"
# This execution environment uses ptrace, which LeakSanitizer cannot inspect.
# AddressSanitizer and UndefinedBehaviorSanitizer remain enabled.
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$test_binary"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/src/Protocol.cpp" "$firmware_dir/src/Link.cpp" \
  "$firmware_dir/test/link_test.cpp" -lcrypto -o "$link_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$link_binary"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/src/Protocol.cpp" "$firmware_dir/src/AppProtocol.cpp" \
  "$firmware_dir/test/app_protocol_test.cpp" -o "$app_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$app_binary"
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/test/identity_test.cpp" -o "$identity_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$identity_binary"
afc_binary="$(mktemp /tmp/reolink-afc-test.XXXXXX)"
trap 'rm -f -- "$test_binary" "$link_binary" "$app_binary" "$identity_binary" "$afc_binary"' EXIT
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/src/Afc.cpp" "$firmware_dir/test/afc_test.cpp" -o "$afc_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$afc_binary"
band_binary="$(mktemp /tmp/reolink-band-test.XXXXXX)"
trap 'rm -f -- "$test_binary" "$link_binary" "$app_binary" "$identity_binary" "$afc_binary" "$band_binary"' EXIT
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" "$firmware_dir/test/band_test.cpp" -o "$band_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$band_binary"
# ArduinoJson comes from PlatformIO's library cache (run `pio run` or `pio pkg install` once).
arduinojson="$firmware_dir/.pio/libdeps/esp32dev/ArduinoJson/src"
if [ ! -f "$arduinojson/ArduinoJson.h" ]; then
  echo "ArduinoJson not found in .pio/libdeps; run 'pio pkg install' in firmware/ first" >&2
  exit 1
fi
features_binary="$(mktemp /tmp/reolink-features-test.XXXXXX)"
trap 'rm -f -- "$test_binary" "$link_binary" "$app_binary" "$identity_binary" "$afc_binary" "$band_binary" "$features_binary"' EXIT
g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I "$firmware_dir/include" -isystem "$arduinojson" "$firmware_dir/src/Protocol.cpp" \
  "$firmware_dir/src/Auth.cpp" "$firmware_dir/src/RingLog.cpp" "$firmware_dir/src/ChimeControl.cpp" \
  "$firmware_dir/src/WifiConfig.cpp" "$firmware_dir/test/features_test.cpp" -lcrypto -o "$features_binary"
ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$features_binary"
# Replay the libFuzzer corpus (fuzz/corpus, grown by tools/fuzz.sh) without clang.
declare -A fuzz_sources=(
  [protocol]="src/Protocol.cpp"
  [link]="src/Protocol.cpp src/Link.cpp src/AppProtocol.cpp"
  [app_protocol]="src/Protocol.cpp src/AppProtocol.cpp"
)
fuzz_binary="$(mktemp /tmp/reolink-fuzz-replay.XXXXXX)"
trap 'rm -f -- "$test_binary" "$link_binary" "$app_binary" "$identity_binary" "$afc_binary" "$band_binary" "$features_binary" "$fuzz_binary"' EXIT
for target in protocol link app_protocol; do
  srcs=(); for s in ${fuzz_sources[$target]}; do srcs+=("$firmware_dir/$s"); done
  g++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer -g -I "$firmware_dir/include" "${srcs[@]}" \
    "$firmware_dir/fuzz/${target}_fuzz.cpp" "$firmware_dir/fuzz/replay.cpp" -o "$fuzz_binary"
  printf 'Fuzz corpus %s: ' "$target"
  ASAN_OPTIONS="detect_leaks=0:${ASAN_OPTIONS:-}" "$fuzz_binary" "$firmware_dir/fuzz/corpus/$target"
done
