#!/usr/bin/env bash
# @file enable_port_502.sh
# @brief Grant only CAP_NET_BIND_SERVICE so the gateway may bind TCP port 502.
# @author Prathamesh Chipkar
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="$ROOT/build/ek9500_modbus_tcp_gateway"
if [[ ! -x "$BINARY" ]]; then
    echo "Gateway executable not found. Run ./scripts/build.sh first." >&2
    exit 1
fi
command -v setcap >/dev/null 2>&1 || { echo "setcap not installed (install libcap)." >&2; exit 1; }
command -v getcap >/dev/null 2>&1 || { echo "getcap not installed (install libcap)." >&2; exit 1; }
sudo setcap 'cap_net_bind_service=+ep' "$BINARY"
getcap "$BINARY"
