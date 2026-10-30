#!/usr/bin/env bash
# @file verify.sh
# @brief Reproducible host-side verification for the consolidated gateway release.
# @author Prathamesh Chipkar
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FULL=false
[[ "${1:-}" == "--full" ]] && FULL=true
if [[ $# -gt 0 && "${1:-}" != "--full" ]]; then
    echo "Usage: ./scripts/verify.sh [--full]" >&2
    exit 2
fi

"$ROOT/scripts/build.sh" --clean

for script in "$ROOT"/scripts/*.sh; do
    bash -n "$script"
done

python3 -m py_compile "$ROOT/tools/ek9500_modbus_live_monitor.py" "$ROOT/tools/stm32_f407_lan8720_contract_test.py"
python3 "$ROOT/tools/ek9500_modbus_live_monitor.py" --self-test
python3 "$ROOT/tools/stm32_f407_lan8720_contract_test.py" --self-test
"$ROOT/build/ek9500_modbus_tcp_gateway" --version

if [[ "$FULL" == true ]]; then
    rm -rf "$ROOT/build-analyzer" "$ROOT/build-sanitizers"

    cmake -S "$ROOT" -B "$ROOT/build-analyzer" -DCMAKE_C_FLAGS="-fanalyzer"
    cmake --build "$ROOT/build-analyzer" --parallel
    ctest --test-dir "$ROOT/build-analyzer" --output-on-failure

    cmake -S "$ROOT" -B "$ROOT/build-sanitizers" \
        -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
    cmake --build "$ROOT/build-sanitizers" --parallel
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
    UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        ctest --test-dir "$ROOT/build-sanitizers" --output-on-failure
fi

echo "Host verification complete. Physical EK9500/electrical commissioning remains separate."
