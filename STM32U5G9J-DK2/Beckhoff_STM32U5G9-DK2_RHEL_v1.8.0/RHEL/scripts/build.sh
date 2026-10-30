#!/usr/bin/env bash
# @file build.sh
# @brief Configure, compile and execute the host regression suite.
# @author Prathamesh Chipkar
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/build"
CLEAN=false
BUILD_DOCS=false

usage() {
    cat <<'USAGE'
Usage: ./scripts/build.sh [--clean] [--docs]

  --clean  Remove the existing build directory before configuring.
  --docs   Generate Doxygen HTML after the build (requires doxygen).
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true ;;
        --docs) BUILD_DOCS=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if [[ "$CLEAN" == true ]]; then
    rm -rf "$BUILD_DIR"
fi

cmake -S "$ROOT" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure

if [[ "$BUILD_DOCS" == true ]]; then
    if ! command -v doxygen >/dev/null 2>&1; then
        echo "Doxygen is not installed. On RHEL: sudo dnf install doxygen" >&2
        exit 1
    fi
    cmake --build "$BUILD_DIR" --target docs
    echo "Doxygen HTML: $ROOT/docs/html/index.html"
fi
