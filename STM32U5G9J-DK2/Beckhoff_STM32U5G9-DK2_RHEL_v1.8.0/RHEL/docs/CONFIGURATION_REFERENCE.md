# Configuration reference

## Gateway

`config/gateway.conf` defines the Modbus listeners, ACL file, PID file and the single `[route.coupler]` route. The default route uses Unit ID 1, `192.168.1.1`, and `config/coupler.conf`.

## Coupler PDO

`config/coupler.conf` is the authoritative hardware profile.

| Terminal | Position | Direction | Byte offset | Size |
|---|---:|---|---:|---:|
| EL1409 DI | 1 | input | 4 | 2 |
| EL2409 DO | 2 | output | 4 | 2 |
| EL3074 AI | 3 | input | 6 | 16 |
| EL4074 status | 4 | input | 22 | 8 |
| EL4074 AO | 4 | output | 6 | 16 |

Input Assembly 129 is 30 bytes. Output Assembly 130 is 22 bytes. Strict size checking is enabled; do not disable it to hide a terminal-order or PDO mismatch.

## Access control

`config/masters.conf` contains allowed Modbus clients as `name|IPv4|max_connections|max_requests_per_second`. The STM32 default is `192.168.1.60`, one connection and 500 requests/s.
