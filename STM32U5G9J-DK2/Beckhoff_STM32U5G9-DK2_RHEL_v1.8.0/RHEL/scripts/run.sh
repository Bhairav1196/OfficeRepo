#!/usr/bin/env bash
# @file run.sh
# @brief Configure permitted masters and start the single EK9500 gateway.
# @author Prathamesh Chipkar
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BINARY="$ROOT/build/ek9500_modbus_tcp_gateway"
CONFIG="$ROOT/config/gateway.conf"
MASTERCTL="$ROOT/scripts/masterctl.sh"
NON_INTERACTIVE=false

if [[ "${1:-}" == "--non-interactive" ]]; then
    NON_INTERACTIVE=true
elif [[ $# -ne 0 ]]; then
    echo "Usage: ./scripts/run.sh [--non-interactive]" >&2
    exit 2
fi

if [[ ! -x "$BINARY" ]]; then
    echo "Gateway executable not found. Run ./scripts/build.sh first." >&2
    exit 1
fi
mkdir -p "$ROOT/runtime"

primary_port="$(awk -F= '/^[[:space:]]*port[[:space:]]*=/{gsub(/[[:space:]]/,"",$2); print $2; exit}' "$CONFIG")"
secondary_port="$(awk -F= '/^[[:space:]]*secondary_port[[:space:]]*=/{gsub(/[[:space:]]/,"",$2); print $2; exit}' "$CONFIG")"
secondary_port="${secondary_port:-0}"

# Any configured listener below 1024 requires CAP_NET_BIND_SERVICE for a
# non-root process. Check both ports so a future configuration can safely make
# 502 the primary listener without weakening the startup guard.
if [[ "$(id -u)" -ne 0 ]]; then
    for listener_port in "$primary_port" "$secondary_port"; do
        if [[ "$listener_port" -gt 0 && "$listener_port" -lt 1024 ]]; then
            if ! command -v getcap >/dev/null 2>&1 || \
               ! getcap "$BINARY" 2>/dev/null | grep -q 'cap_net_bind_service'; then
                cat >&2 <<INFO
TCP port $listener_port is a privileged port on Linux.
Grant only the bind-service capability to this gateway binary, then rerun:

  sudo setcap 'cap_net_bind_service=+ep' "$BINARY"
  getcap "$BINARY"

Expected:
  $BINARY cap_net_bind_service=ep

Do not run the complete gateway as root merely to bind a privileged Modbus TCP port.
INFO
                exit 1
            fi
        fi
    done
fi

if [[ "$NON_INTERACTIVE" == false && -t 0 ]]; then
    while true; do
        echo
        "$MASTERCTL" list
        cat <<'MENU'

Master access control:
  1) Add a Modbus TCP master
  2) Remove a Modbus TCP master
  3) Remove all masters
  4) Start gateway with the displayed list
  5) Exit

Notes:
  - For STM32F407/LAN8720 use: ./scripts/masterctl.sh add-stm32 192.168.1.19
  - MAX_REQ_PER_SEC is a rate limit, not a TCP port. Use 500 for the STM32 20 ms polling profile.
MENU
        read -r -p "Selection [4]: " choice
        case "${choice:-4}" in
            1)
                read -r -p "Master name (example ModScan_PC, Beijer_HMI, STM32F407_LAN8720): " name
                read -r -p "Master IPv4 address: " ip
                read -r -p "Maximum TCP connections [2]: " max_connections
                read -r -p "MAX_REQ_PER_SEC [default; use 500 for STM32F407_LAN8720]: " max_requests_per_second
                if [[ -n "${max_requests_per_second:-}" ]]; then
                    "$MASTERCTL" add "$name" "$ip" "${max_connections:-2}" "$max_requests_per_second"
                else
                    "$MASTERCTL" add "$name" "$ip" "${max_connections:-2}"
                fi
                ;;
            2)
                read -r -p "Master name or IPv4 to remove: " key
                "$MASTERCTL" remove "$key"
                ;;
            3)
                read -r -p "Remove every allowed master? Type YES: " confirmation
                [[ "$confirmation" == "YES" ]] && "$MASTERCTL" clear || echo "Cancelled."
                ;;
            4)
                break
                ;;
            5)
                exit 0
                ;;
            *)
                echo "Invalid selection." >&2
                ;;
        esac
    done
fi

cat <<INFO
Starting gateway:
  Modbus primary listener   : RHEL IP, TCP port $primary_port (ModScan may continue here)
  Modbus secondary listener : RHEL IP, TCP port $secondary_port (Beijer/standard Modbus TCP)
  Unit ID 1                 : EK9500 192.168.1.1

Both Modbus ports use the SAME Unit ID 1 register map, master ACL and global client limit.
While running, use another RHEL terminal:
  ./scripts/masterctl.sh add <name> <IPv4> [max_connections] [max_requests_per_second]
  ./scripts/masterctl.sh remove <name-or-IPv4>
  ./scripts/masterctl.sh list

Changes are applied by SIGHUP. Other allowed ModScan/HMI connections remain open.
INFO

exec "$BINARY" "$CONFIG"
