#!/usr/bin/env bash
# @file masterctl.sh
# @brief Add, remove, list, clear, profile, and live-reload permitted Modbus TCP masters.
# @author Prathamesh Chipkar
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "$(basename "$0")" == "ek9500-masterctl" ]]; then
    DEFAULT_ACL_FILE="/etc/ek9500-modbus-gateway/masters.conf"
    DEFAULT_PID_FILE="/run/ek9500-modbus-gateway/gateway.pid"
else
    DEFAULT_ACL_FILE="$ROOT/config/masters.conf"
    DEFAULT_PID_FILE="$ROOT/runtime/gateway.pid"
fi
ACL_FILE="${MASTER_ACL_FILE:-$DEFAULT_ACL_FILE}"
PID_FILE="${GATEWAY_PID_FILE:-$DEFAULT_PID_FILE}"
LOCK_FILE="${ACL_FILE}.lock"

usage() {
    cat <<'USAGE'
Usage:
  ./scripts/masterctl.sh list
  ./scripts/masterctl.sh add <name> <IPv4> [max_connections] [max_requests_per_second]
  ./scripts/masterctl.sh add-stm32 <IPv4> [max_connections] [max_requests_per_second]
  ./scripts/masterctl.sh add-stm32-lan8720 <IPv4> [max_connections] [max_requests_per_second]
  ./scripts/masterctl.sh add-stm32-w5100 <IPv4> [max_connections] [max_requests_per_second]
  ./scripts/masterctl.sh remove <name-or-IPv4>
  ./scripts/masterctl.sh clear
  ./scripts/masterctl.sh reload

ACL format:
  name|IPv4|max_connections[|max_requests_per_second]

When max_requests_per_second is omitted or zero, the gateway-wide limit from
[modbus] is used. The STM32 profile defaults to one persistent connection and
500 requests/s, providing headroom for a 50 Hz multi-block polling cycle.

The allowlist is reloaded automatically after add, add-stm32, remove, or clear
when the running gateway PID is available. `add-stm32` is the current
STM32F407/LAN8720 profile; `add-stm32-w5100` is retained only for v1.4.0 legacy
commissioning. Removing a master disconnects all of its current Modbus TCP
connections without closing unrelated masters.
USAGE
}

is_name() { [[ "$1" =~ ^[A-Za-z0-9_.-]{1,63}$ ]]; }

is_ipv4() {
    local value="$1" a b c d extra octet
    IFS='.' read -r a b c d extra <<<"$value"
    [[ -z "${extra:-}" ]] || return 1
    for octet in "$a" "$b" "$c" "$d"; do
        [[ "$octet" =~ ^[0-9]{1,3}$ ]] || return 1
        ((10#$octet >= 0 && 10#$octet <= 255)) || return 1
    done
    ((10#$a < 224)) || return 1
    [[ "$value" != "0.0.0.0" && "$value" != "255.255.255.255" ]]
}

is_connection_count() { [[ "$1" =~ ^[0-9]+$ ]] && ((10#$1 >= 1 && 10#$1 <= 32)); }
is_rate() { [[ "$1" =~ ^[0-9]+$ ]] && ((10#$1 >= 0 && 10#$1 <= 4294967295)); }

ensure_files() {
    mkdir -p "$(dirname "$ACL_FILE")" "$(dirname "$PID_FILE")"
    if [[ ! -e "$ACL_FILE" ]]; then
        cat >"$ACL_FILE" <<'HEADER'
# Modbus TCP master allowlist
# Format: name|IPv4|max_connections[|max_requests_per_second]
HEADER
        chmod 0640 "$ACL_FILE"
    fi
}

reload_if_running() {
    local pid=""
    if [[ -r "$PID_FILE" ]]; then read -r pid <"$PID_FILE" || true; fi
    if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
        kill -HUP "$pid"
        echo "Reload requested for gateway PID $pid."
    else
        echo "Gateway is not running; the updated list will be loaded at next start."
    fi
}

list_masters() {
    ensure_files
    printf '%-24s %-15s %-15s %s\n' "NAME" "IPV4" "MAX_CONNECTIONS" "MAX_REQ_PER_SEC"
    printf '%-24s %-15s %-15s %s\n' "------------------------" "---------------" "---------------" "---------------"
    awk -F'|' '
        /^[[:space:]]*($|#|;)/ { next }
        NF == 3 { printf "%-24s %-15s %-15s %s\n", $1, $2, $3, "default"; count++ }
        NF == 4 { printf "%-24s %-15s %-15s %s\n", $1, $2, $3, ($4 == 0 ? "default" : $4); count++ }
        END { if (count == 0) print "<no remote masters allowed>" }
    ' "$ACL_FILE"
}

add_master() {
    local name="$1" ip="$2" max_connections="$3" max_rate="$4" temp
    is_name "$name" || { echo "Invalid name. Use letters, digits, dot, dash, or underscore." >&2; exit 2; }
    is_ipv4 "$ip" || { echo "Invalid unicast IPv4 address: $ip" >&2; exit 2; }
    is_connection_count "$max_connections" || { echo "max_connections must be 1..32." >&2; exit 2; }
    is_rate "$max_rate" || { echo "max_requests_per_second must be 0..4294967295." >&2; exit 2; }
    ensure_files
    exec 9>"$LOCK_FILE"; flock -x 9
    if awk -F'|' -v name="$name" -v ip="$ip" '
        /^[[:space:]]*($|#|;)/ { next }
        ($1 == name) || ($2 == ip) { found=1 }
        END { exit(found ? 0 : 1) }
    ' "$ACL_FILE"; then
        echo "A master with name '$name' or IPv4 '$ip' already exists." >&2; exit 3
    fi
    temp="$(mktemp "${ACL_FILE}.tmp.XXXXXX")"
    cat "$ACL_FILE" >"$temp"
    if ((10#$max_rate == 0)); then
        printf '%s|%s|%s\n' "$name" "$ip" "$max_connections" >>"$temp"
    else
        printf '%s|%s|%s|%s\n' "$name" "$ip" "$max_connections" "$max_rate" >>"$temp"
    fi
    chmod 0640 "$temp"; mv -f "$temp" "$ACL_FILE"
    if ((10#$max_rate == 0)); then
        echo "Added master $name ($ip), maximum connections=$max_connections, rate=gateway-default."
    else
        echo "Added master $name ($ip), maximum connections=$max_connections, rate=$max_rate req/s."
    fi
    reload_if_running
}

remove_master() {
    local key="$1" temp
    ensure_files
    exec 9>"$LOCK_FILE"; flock -x 9
    if ! awk -F'|' -v key="$key" '
        /^[[:space:]]*($|#|;)/ { next }
        ($1 == key) || ($2 == key) { found=1 }
        END { exit(found ? 0 : 1) }
    ' "$ACL_FILE"; then
        echo "No master named or addressed '$key'." >&2; exit 4
    fi
    temp="$(mktemp "${ACL_FILE}.tmp.XXXXXX")"
    awk -F'|' -v key="$key" '
        /^[[:space:]]*($|#|;)/ { print; next }
        ($1 == key) || ($2 == key) { next }
        { print }
    ' "$ACL_FILE" >"$temp"
    chmod 0640 "$temp"; mv -f "$temp" "$ACL_FILE"
    echo "Removed master $key. Existing connections from that IPv4 will close."
    reload_if_running
}

clear_masters() {
    local temp
    ensure_files
    exec 9>"$LOCK_FILE"; flock -x 9
    temp="$(mktemp "${ACL_FILE}.tmp.XXXXXX")"
    cat >"$temp" <<'HEADER'
# Modbus TCP master allowlist
# Format: name|IPv4|max_connections[|max_requests_per_second]
HEADER
    chmod 0640 "$temp"; mv -f "$temp" "$ACL_FILE"
    echo "Removed all allowed masters. New connections will be rejected."
    reload_if_running
}

command="${1:-list}"
case "$command" in
    list)
        [[ $# -eq 1 ]] || { usage; exit 2; }; list_masters ;;
    add)
        [[ $# -ge 3 && $# -le 5 ]] || { usage; exit 2; }
        add_master "$2" "$3" "${4:-2}" "${5:-0}" ;;
    add-stm32|add-stm32-lan8720)
        [[ $# -ge 2 && $# -le 4 ]] || { usage; exit 2; }
        add_master "STM32F407_LAN8720" "$2" "${3:-1}" "${4:-500}" ;;
    add-stm32-w5100)
        [[ $# -ge 2 && $# -le 4 ]] || { usage; exit 2; }
        add_master "STM32U5_W5100" "$2" "${3:-1}" "${4:-500}" ;;
    remove)
        [[ $# -eq 2 ]] || { usage; exit 2; }; remove_master "$2" ;;
    clear)
        [[ $# -eq 1 ]] || { usage; exit 2; }; clear_masters ;;
    reload)
        [[ $# -eq 1 ]] || { usage; exit 2; }; ensure_files; reload_if_running ;;
    help|-h|--help) usage ;;
    *) usage; exit 2 ;;
esac
