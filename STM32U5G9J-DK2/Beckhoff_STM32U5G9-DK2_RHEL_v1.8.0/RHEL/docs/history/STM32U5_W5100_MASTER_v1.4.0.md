# STM32U5G9J-DK2 + W5100 Modbus TCP Master Integration — v1.4.0

## 1. Scope

This document defines the RHEL-facing contract for a new embedded Modbus TCP
master based on:

- STM32U5G9J-DK2
- WIZnet W5100 Arduino-compatible Ethernet shield
- ThreadX/TouchGFX on the STM32 side
- no microSD-card functionality
- persistent Modbus TCP connection to the RHEL gateway
- RHEL gateway v1.4.0 translating Modbus TCP to EtherNet/IP for the EK9500s

The complete communication path is:

```text
STM32U5G9J-DK2
  -> SPI1
  -> W5100 Ethernet controller
  -> 100BASE-TX network
  -> Modbus TCP, persistent TCP socket
  -> RHEL gateway v1.4.0, TCP/502 recommended
  -> shared Modbus data model
  -> EtherNet/IP route workers
  -> EK9500 Unit 1 / Unit 2
  -> Beckhoff EL1409 / EL2409 / EL3074 / EL4074
```

The STM32 never communicates EtherNet/IP directly with the EK9500 in this
architecture. It addresses the RHEL host as its Modbus TCP server.

## 2. SD-card boundary

The RHEL gateway has no SD-card dependency, configuration, protocol, service or
runtime path. The W5100 shield's microSD device is outside the RHEL software
boundary.

For the future STM32 firmware, the SD device shall remain unselected. On common
Arduino-compatible W5100 shields the W5100 and microSD share the SPI bus, so the
STM32 firmware shall keep `SD_CS` deasserted/high before and during W5100 SPI
transactions. No FatFs, block-device, SDMMC or microSD application feature is
required for this project.

## 3. Network role

| Node | Role | Example |
|---|---|---|
| STM32U5 + W5100 | Modbus TCP master/client | `192.168.1.19` |
| RHEL host | Modbus TCP server/gateway | site-specific |
| EK9500 Unit 1 | EtherNet/IP adapter behind RHEL | `192.168.1.1` |
| EK9500 Unit 2 | EtherNet/IP adapter behind RHEL | `192.168.1.3` |

Do not configure the STM32 to open a TCP connection to either EK9500 for this
application. Its TCP peer is the RHEL gateway only.

## 4. Recommended Modbus transport profile

| Parameter | Initial target |
|---|---:|
| TCP port | `502` |
| TCP connections | `1` persistent connection |
| Poll period | `20 ms` |
| Poll frequency | `50 Hz` |
| Reconnect strategy | bounded retry/backoff |
| Modbus transaction IDs | monotonically increment modulo 65536 |
| TCP_NODELAY | enabled on RHEL accepted socket |
| STM32 ACL request ceiling | `500 requests/s` |
| Gateway-wide fallback ceiling | `200 requests/s` |
| RHEL EtherNet/IP route worker | `5 ms` |
| EK9500 requested packet interval | `10 ms` |

A 20 ms master cycle is intentionally slower than the RHEL route worker period
and comparable to the EtherNet/IP process-data cadence. Polling much faster does
not guarantee proportionally newer physical I/O data.

## 5. Register transaction plan

A compact initial 50 Hz cycle can use five requests:

1. Unit 1, FC02, address 0, quantity 16: EL1409 DI.
2. Unit 1, FC04, address 1024, quantity 4: EL3074 engineering mV.
3. Unit 2, FC02, address 0, quantity 16: EL1409 DI.
4. Unit 2, FC03, address 0, quantity 4: EL4074 AO command readback.
5. Unit 2, FC04, address 3072, quantity 4: EL4074 status.

That is approximately 250 requests/s at 50 Hz. Occasional FC05/FC15 DO writes
or FC06/FC16 AO writes fit inside the default STM32 profile of 500 requests/s.

When bandwidth/CPU optimization becomes useful, poll slow-changing status less
frequently than primary process values instead of increasing the base rate.

## 6. Unit-ID contract

- Unit ID `1` always maps to EK9500 `192.168.1.1`.
- Unit ID `2` always maps to EK9500 `192.168.1.3`.

The STM32 shall use the Unit ID in the MBAP header; it shall not infer a coupler
from TCP port number.

## 7. EL4074 command contract

The STM32 writes one signed `int16_t` command per EL4074 channel:

```text
Holding Registers 40001..40004
protocol addresses 0..3
command resolution = 0.01 mA/count
valid range = -2000..+2000
```

Examples:

```text
-2000 -> -20.00 mA -> -0.02000 A REAL32 in EtherNet/IP PDO
 -500 ->  -5.00 mA -> -0.00500 A REAL32
  132 ->  +1.32 mA -> +0.00132 A REAL32
  899 ->  +8.99 mA -> +0.00899 A REAL32
 2000 -> +20.00 mA -> +0.02000 A REAL32
```

The STM32 shall not send IEEE-754 REAL32 over Modbus for AO. The Modbus wire
representation is exactly one 16-bit register per channel.

## 8. STM32 master allowlist profile

Add the actual STM32/W5100 IPv4 address on RHEL:

```bash
./scripts/masterctl.sh add-stm32 192.168.1.19
```

The generated ACL line is:

```text
STM32U5_W5100|192.168.1.19|1|500
```

The four fields are:

```text
name | source IPv4 | maximum simultaneous TCP connections | max requests/s
```

The optional fourth field is new in v1.4.0. Existing three-field entries remain
valid and inherit `max_requests_per_second_per_client` from `gateway.conf`.

Use a single W5100 socket for normal Modbus traffic. A second connection should
not be required for regular operation.

## 9. Why the STM32 profile has a custom request limit

v1.3.0 used one global request-rate ceiling of 200 requests/s. A 50 Hz embedded
poll cycle with five normal reads consumes about 250 requests/s before any
output writes are added. Raising the global limit would also relax ModScan and
Beijer clients.

v1.4.0 therefore adds an optional per-master ceiling. The STM32 gets controlled
headroom while ordinary clients continue to use the conservative global limit.
The ceiling is a protection limit, not a target polling rate.

## 10. Persistent TCP behaviour

The STM32 should:

1. initialize SPI/W5100,
2. obtain/configure its fixed IPv4 parameters,
3. open one TCP socket to RHEL port 502,
4. keep it open,
5. issue Modbus transactions sequentially with unique transaction IDs,
6. validate MBAP transaction ID, protocol ID, Unit ID and PDU length,
7. handle Modbus exception responses,
8. reconnect only after confirmed socket failure/timeout,
9. use bounded retry/backoff rather than reconnecting every poll cycle.

Opening and closing TCP for every register transaction is explicitly not the
intended operating mode.

## 11. RHEL commissioning tool

`tools/stm32_w5100_contract_test.py` emulates the intended persistent master
transaction pattern from a host. It is useful before STM32 firmware is ready.

Offline frame/scaling self-test:

```bash
python3 tools/stm32_w5100_contract_test.py --self-test
```

Read-only live test against the RHEL gateway:

```bash
python3 tools/stm32_w5100_contract_test.py \
  --host <RHEL_IP> --port 502 --period-ms 20 --cycles 100
```

Optional controlled AO write, channel 1 at +1.32 mA:

```bash
python3 tools/stm32_w5100_contract_test.py \
  --host <RHEL_IP> --port 502 --write-ao 1.32 --ao-channel 1 --cycles 1
```

Only use output-write options during controlled commissioning with a safe load.

## 12. Failure handling expectations

The STM32 firmware should distinguish:

- TCP connect timeout/refusal,
- TCP peer close/reset,
- malformed MBAP response,
- transaction-ID mismatch,
- Modbus exception code,
- stale or semantically invalid process data,
- W5100 socket state failure,
- local SPI/W5100 initialization failure.

A communication loss must not be translated into arbitrary output values.
Application safe-state behaviour belongs to the higher-level system design.

## 13. Performance boundary

This architecture is appropriate for supervisory and industrial I/O exchange at
roughly 10–100 Hz. It is not a deterministic sub-millisecond motion-control
fieldbus. End-to-end freshness includes TCP, RHEL scheduling/data-model access,
EtherNet/IP cadence and EK9500 terminal update timing.

## 14. Acceptance checks

Before declaring the STM32 path commissioned:

- verify the STM32 source IPv4 is allowlisted,
- verify one persistent TCP/502 connection remains stable,
- verify Unit 1 and Unit 2 reads simultaneously,
- verify negative signed values are decoded correctly,
- verify `132` AO command becomes `0.00132 A` in the Beckhoff web console,
- verify the physical current is approximately `1.32 mA`,
- verify reconnection after unplug/replug or RHEL service restart,
- verify other permitted masters remain responsive during STM32 polling,
- verify the SD chip is never selected by STM32 firmware,
- verify no microSD/FatFs dependency exists in the firmware configuration.
