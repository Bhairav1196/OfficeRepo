#!/usr/bin/env bash
# @file test_masterctl.sh
# @brief Regression test for current and legacy STM32 master profile helpers.
# @author Prathamesh Chipkar
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP="$(mktemp -d /tmp/ek9500-masterctl-test.XXXXXX)"
trap 'rm -rf "$TMP"' EXIT
export MASTER_ACL_FILE="$TMP/masters.conf"
export GATEWAY_PID_FILE="$TMP/gateway.pid"

"$ROOT/scripts/masterctl.sh" add-stm32 192.168.1.19 >"$TMP/add.out"
grep -qx 'STM32F407_LAN8720|192.168.1.19|1|500' "$MASTER_ACL_FILE"
"$ROOT/scripts/masterctl.sh" list >"$TMP/list.out"
grep -q 'STM32F407_LAN8720' "$TMP/list.out"
grep -q '500' "$TMP/list.out"

"$ROOT/scripts/masterctl.sh" remove STM32F407_LAN8720 >"$TMP/remove.out"
! grep -q '^STM32F407_LAN8720|' "$MASTER_ACL_FILE"

"$ROOT/scripts/masterctl.sh" add-stm32-w5100 192.168.1.61 >"$TMP/legacy.out"
grep -qx 'STM32U5_W5100|192.168.1.61|1|500' "$MASTER_ACL_FILE"

printf '%s\n' 'masterctl STM32 profile tests passed'
