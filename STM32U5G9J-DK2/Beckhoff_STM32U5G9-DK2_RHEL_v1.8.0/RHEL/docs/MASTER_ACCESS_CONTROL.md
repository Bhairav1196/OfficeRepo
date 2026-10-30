# Master Access Control — v1.5.0

`config/masters.conf` is an operational source-IPv4 allowlist shared by both
Modbus TCP listeners.

## Format

```text
name|IPv4|max_connections[|max_requests_per_second]
```

The fourth field was introduced in v1.4.0 and remains optional in v1.5.0. If it is missing or zero, that host uses
the gateway-wide `max_requests_per_second_per_client` limit. Existing v1.3.0
three-field ACL files therefore remain valid.

Examples:

```text
ModScan_PC|192.168.1.10|8
Beijer_HMI|192.168.1.50|2
STM32F407_LAN8720|192.168.1.19|1|500
```

Blank/comment lines are allowed. Duplicate names/IPs or malformed entries are
rejected. An empty valid file denies every Modbus master.

## Runtime management

```bash
./scripts/masterctl.sh list
./scripts/masterctl.sh add <name> <IPv4> [max_connections] [max_requests_per_second]
./scripts/masterctl.sh add-stm32 <IPv4> [max_connections] [max_requests_per_second]
./scripts/masterctl.sh remove <name-or-IPv4>
./scripts/masterctl.sh clear
./scripts/masterctl.sh reload
```

`add-stm32` defaults to:

```text
name                    STM32F407_LAN8720
max_connections         1
max_requests_per_second 500
```

The script updates the file atomically and signals the gateway with `SIGHUP`.
The gateway parses the replacement before applying it. If parsing fails, the
old ACL remains active.

## Connection and rate effects

- Newly allowed host: may establish connections immediately after reload.
- Removed host: existing connections from that IP are closed.
- Unchanged allowed host: connections remain open.
- `max_connections`: limits simultaneous TCP sessions from that source IP.
- custom request rate: applied to new connections and updated in-place for
  retained connections on a successful ACL reload.
- omitted/zero custom request rate: uses the gateway-wide default.

The request-rate value is a protection ceiling, not a scheduler. For the
STM32F407/LAN8720 master, the normal target remains a 20 ms cycle; 500 req/s simply
leaves headroom for several reads and occasional output writes.

## Security boundary

This is network-host authorization, not user/application authentication. Two
programs sharing one source IP are indistinguishable. It also does not implement
per-output write ownership. Network segmentation/firewalling and system-level
hardening remain required for production deployment.
