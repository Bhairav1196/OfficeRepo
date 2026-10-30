# STM32F407/LAN8720 Master Contract — v1.5.0 RC5

## Network

```text
STM32 IP      : 192.168.1.19/24
RHEL IP       : 192.168.1.7/24
Modbus TCP    : 502
Master ACL    : STM32F407_LAN8720|192.168.1.19|1|500
```

## Polling pattern

| Unit | FC | Address | Qty | Meaning |
|---:|---:|---:|---:|---|
| 1 | 02 | 0 | 16 | EL1409 DI |
| 1 | 01 | 0 | 16 | EL2409 DO readback |
| 1 | 04 | 1024 | 4 | EL3074 engineering values |
| 2 | 02 | 0 | 16 | EL1409 DI |
| 2 | 01 | 0 | 16 | EL2409 DO readback |
| 2 | 03 | 0 | 4 | EL4074 AO command/readback |
| 2 | 04 | 3072 | 4 | EL4074 status |

## Analog profile contract

STM32 `Core/Inc/network_config.h` and RHEL config must match.

| Profile | AI scale | AO command scale |
|---|---|---|
| ±10 V | mV | 0.01 V/count |
| ±20 mA | 0.01 mA/count | 0.01 mA/count |

The generated RC5 default keeps AI at ±10 V and AO at ±20 mA.
