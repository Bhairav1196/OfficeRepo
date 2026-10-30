# Dual EK9500 Modbus TCP Gateway v1.5.0 RC5

## STM32F407/LAN8720 Modbus TCP Master baseline — ±10 V / ±20 mA analog profiles

This release is the RHEL/Linux side of the Beckhoff EK9500 gateway paired with
the STM32F407VGT6 + LAN8720A ThreadX/NetX Duo Modbus TCP Master. The working
RC3/RC4 DI/DO/AI/AO communication path is retained. RC5 changes the requested
current profile from **4..20 mA** to **±20 mA**.

Supported primary analog profiles:

1. `bipolar_10v` = ±10 V
2. `bipolar_20ma` = ±20 mA

The old `current_4_20ma` parser path remains only as deprecated compatibility
for older RC4 experiments. Do not use it for the current project unless you
explicitly need legacy 4..20 mA behaviour.

```text
STM32F407 + LAN8720              ModScan / Beijer / CLI tools
192.168.1.19                             |
       | TCP 502                         | TCP 1502 or 502
       +---------------+-----------------+
                       |
              RHEL gateway 192.168.1.7
                       |
          +------------+------------+
          |                         |
       Unit ID 1                 Unit ID 2
   EK9500 192.168.1.1       EK9500 192.168.1.3
   EL1409 + EL2409 +        EL1409 + EL2409 +
        EL3074                   EL4074
     DI + DO + AI             DI + DO + AO
```

## 1. Locked network contract

| Item | Value |
|---|---|
| RHEL gateway Ethernet IP | `192.168.1.7/24` |
| STM32F407/LAN8720 IP | `192.168.1.19/24` |
| STM32 Modbus role | Master / TCP client |
| RHEL Modbus role | Server / TCP listener |
| Human/PC test port | `1502` |
| STM32 target port | `502` |
| Unit ID 1 route | EK9500 `192.168.1.1` |
| Unit ID 2 route | EK9500 `192.168.1.3` |
| Required ACL line | `STM32F407_LAN8720|192.168.1.19|1|500` |

The Modbus TCP port is configured in `config/gateway.conf`. The master allowlist
is configured in `config/masters.conf`. Do not put a TCP port in `masters.conf`;
the fourth field is maximum requests per second.

## 2. Hardware routes and assemblies

| Unit ID | Coupler | Terminal train | Input Assembly 129 | Output Assembly 130 |
|---:|---|---|---:|---:|
| 1 | `192.168.1.1` | EL1409 → EL2409 → EL3074 | 22 B | 6 B |
| 2 | `192.168.1.3` | EL1409 → EL2409 → EL4074 | 14 B | 22 B |

Unit 1 input image:

```text
bytes 0..3    EK9500 header/control area
bytes 4..5    EL1409 DI bitmap
bytes 6..21   EL3074: 4 x (status16 + value16)
```

Unit 1 output image:

```text
bytes 0..3    EK9500 header/control area
bytes 4..5    EL2409 DO bitmap
```

Unit 2 input image:

```text
bytes 0..3    EK9500 header/control area
bytes 4..5    EL1409 DI bitmap
bytes 6..13   EL4074: 4 x status16
```

Unit 2 output image:

```text
bytes 0..3    EK9500 header/control area
bytes 4..5    EL2409 DO bitmap
bytes 6..21   EL4074: 4 x REAL32, little-endian, configured SI engineering units
```

`strict_assembly_size = true` is intentional. If an EK9500 assembly size does
not match the selected terminal train, treat it as a topology/configuration
fault instead of blindly changing the configured size.

## 3. Modbus register map

All protocol addresses are zero-based. Traditional display references are shown
for ModScan/HMI convenience.

### Unit ID 1 — DI/DO/AI — EK9500 `192.168.1.1`

| Table | Protocol address | Display | Qty | FC | Meaning |
|---|---:|---:|---:|---|---|
| Coils | 0 | 00001 | 16 | 01, 05, 15 | EL2409 DO1..DO16 |
| Discrete Inputs | 0 | 10001 | 16 | 02 | EL1409 DI1..DI16 |
| Input Registers | 0 | 30001 | 4 | 04 | EL3074 raw signed sample |
| Input Registers | 1024 | 31025 | 4 | 04 | EL3074 engineering scaled value |
| Input Registers | 2048 | 32049 | 4 | 04 | EL3074 status word |

EL3074 engineering block at protocol address `1024`:

| `analog_mode` | Register scale | Example |
|---|---|---|
| `bipolar_10v` | signed mV | `8190` = `+8.190 V` |
| `bipolar_20ma` | signed 0.01 mA/count | `1234` = `+12.34 mA` |

### Unit ID 2 — DI/DO/AO — EK9500 `192.168.1.3`

| Table | Protocol address | Display | Qty | FC | Meaning |
|---|---:|---:|---:|---|---|
| Coils | 0 | 00001 | 16 | 01, 05, 15 | EL2409 DO1..DO16 |
| Discrete Inputs | 0 | 10001 | 16 | 02 | EL1409 DI1..DI16 |
| Holding Registers | 0 | 40001 | 4 | 03, 06, 16, 23 | EL4074 AO command/readback |
| Input Registers | 3072 | 33073 | 4 | 04 | EL4074 status word |

EL4074 AO Holding Registers at protocol address `0`:

| `analog_mode` | Register scale | Valid write range | Example |
|---|---|---:|---|
| `bipolar_10v` | signed 0.01 V/count | `-1000..+1000` | `550` = `+5.50 V` |
| `bipolar_20ma` | signed 0.01 mA/count | `-2000..+2000` | `-500` = `-5.00 mA` |
| `bipolar_20ma` | signed 0.01 mA/count | `-2000..+2000` | `132` = `+1.32 mA` |
| `bipolar_20ma` | signed 0.01 mA/count | `-2000..+2000` | `2000` = `+20.00 mA` |

## 4. Selecting analog module profile

The RHEL configuration and STM32 firmware configuration must match the actual
Beckhoff terminal/channel setting.

### Unit 1 EL3074 AI profile

Edit `config/coupler_ai.conf`, section `[terminal.AI1]`.

For ±10 V:

```ini
analog_mode = bipolar_10v
engineering_min = -10.0
engineering_max = 10.0
```

For ±20 mA:

```ini
analog_mode = bipolar_20ma
engineering_min = -0.020
engineering_max = 0.020
```

### Unit 2 EL4074 AO profile

Edit `config/coupler_ao.conf`, section `[terminal.AO1]`.

For ±10 V:

```ini
analog_mode = bipolar_10v
engineering_min = -10.0
engineering_max = 10.0
```

For ±20 mA:

```ini
analog_mode = bipolar_20ma
engineering_min = -0.020
engineering_max = 0.020
```

The generated RC5 package defaults to:

```text
Unit 1 EL3074 AI : bipolar_10v
Unit 2 EL4074 AO : bipolar_20ma
```

Match the STM32 file `Core/Inc/network_config.h`:

```c
#define MODBUS_AI_ANALOG_PROFILE MODBUS_ANALOG_PROFILE_BIPOLAR_10V
#define MODBUS_AO_ANALOG_PROFILE MODBUS_ANALOG_PROFILE_BIPOLAR_20MA
```

## 5. Build and run

Install tools:

```bash
sudo dnf install -y gcc cmake make libcap python3
```

Build:

```bash
cd ~/Downloads/ek9500_dual_modbus_tcp_gateway_v1.5.0_STM32F407_LAN8720_ready_RC5
./scripts/build.sh --clean
```

Ensure the STM32 master is in the ACL:

```bash
./scripts/masterctl.sh remove STM32F407_LAN8720 || true
./scripts/masterctl.sh add STM32F407_LAN8720 192.168.1.19 1 500
./scripts/masterctl.sh list
```

Expected line:

```text
STM32F407_LAN8720        192.168.1.19    1    500
```

Enable non-root binding to TCP/502:

```bash
sudo setcap 'cap_net_bind_service=+ep' ./build/ek9500_modbus_tcp_gateway
getcap ./build/ek9500_modbus_tcp_gateway
```

Start the gateway:

```bash
./scripts/run.sh
```

Choose the menu option to start the gateway with the displayed master list.

## 6. Linux terminal Modbus commands

The helper CLI is:

```bash
tools/modbus_cli.py
```

Use `--host 127.0.0.1 --port 1502` when testing locally on RHEL. Use `--port 502`
only when you deliberately want to test the privileged listener.

### Read DI

Read Unit 1 digital inputs:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-di --unit 1
```

Read Unit 2 digital inputs:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-di --unit 2
```

### Read DO

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-do --unit 1
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-do --unit 2
```

### Write DO high/low

Set Unit 1 DO1 high:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-do --unit 1 --channel 1 --value on
```

Set Unit 1 DO1 low:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-do --unit 1 --channel 1 --value off
```

Set Unit 2 DO5 high:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-do --unit 2 --channel 5 --value on
```

Write all 16 DO channels with a mask:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-do-mask --unit 1 --mask 0x0001
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-do-mask --unit 2 --mask 0x00FF
```

### Read AI

Voltage mode:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ai --unit 1 --profile voltage
```

Current mode:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ai --unit 1 --profile current
```

Raw/status:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ai-raw --unit 1
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ai-status --unit 1
```

### Write AO voltage values

These commands are valid when `config/coupler_ao.conf` is set to
`analog_mode = bipolar_10v` and STM32 is built with
`MODBUS_AO_ANALOG_PROFILE = MODBUS_ANALOG_PROFILE_BIPOLAR_10V`.

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-voltage --channel 1 --volts 5.50
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-voltage --channel 2 --volts -10.00
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-voltage --channel 4 --volts 0.00
```

### Write AO ±20 mA values

These commands are valid when `config/coupler_ao.conf` is set to
`analog_mode = bipolar_20ma` and STM32 is built with
`MODBUS_AO_ANALOG_PROFILE = MODBUS_ANALOG_PROFILE_BIPOLAR_20MA`.

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-current --channel 1 --ma -20.00
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-current --channel 1 --ma -5.00
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-current --channel 1 --ma 1.32
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-current --channel 4 --ma 20.00
```

Write all four AO current channels in one FC16 transaction:

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 write-ao-current-all --ma -20.00 -5.00 0.00 20.00
```

### Read AO command/readback and status

```bash
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ao --profile current
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ao --profile voltage
tools/modbus_cli.py --host 127.0.0.1 --port 1502 read-ao-status
```

## 7. STM32 validation from RHEL

Check link/IP:

```bash
ping 192.168.1.19
```

Watch Modbus TCP traffic:

```bash
sudo tcpdump -i <ethernet-interface> -nn -X host 192.168.1.19 and port 502
```

Watch all gateway Modbus traffic locally:

```bash
sudo tcpdump -i any -nn -X port 1502 or port 502
```

Expected STM32 live behaviour:

```text
Accepted Modbus master STM32F407_LAN8720 from 192.168.1.19
FC02/FC01/FC03/FC04 requests repeat according to the STM32 poll table
```

## 8. Key learnings preserved in this release

- STM32F407 has a native Ethernet MAC; LAN8720 is the RMII PHY.
- STM32U5G9 cannot directly use LAN8720 because it lacks a native Ethernet MAC.
- LAN8720 MDIO PHY address measured on the bench is `1`.
- LAN8720 PHY ID observed: `0x0007 / 0xC0F1`.
- `TIM6` is used for HAL tick; `SysTick` is used by ThreadX.
- `TIM6_DAC_IRQHandler()` must call `HAL_TIM_IRQHandler(&htim6)` or HAL timeouts/delays stall.
- RMII requires a valid 50 MHz reference clock on PA1.
- STM32F407 Ethernet DMA buffers must not be placed in CCM RAM.
- RHEL listens on `1502` for easy local tests and `502` for standard Modbus TCP.
- TCP/502 on Linux requires `cap_net_bind_service` if the gateway is not run as root.
- `masters.conf` contains allowed clients, not ports.
- `STM32F407_LAN8720|192.168.1.19|1|500` is the expected STM32 ACL entry.
- For RC5, the active analog choices are ±10 V and ±20 mA.

## 9. Acceptance criteria

A healthy bench run shows:

```text
RHEL gateway started on 1502 and 502
STM32 accepted as STM32F407_LAN8720 from 192.168.1.19
No master ACL rejection
DI/DO polling works on Unit 1 and Unit 2
Unit 1 AI values decode according to coupler_ai.conf
Unit 2 AO commands write according to coupler_ao.conf
AO status read succeeds at input-register protocol address 3072
```
