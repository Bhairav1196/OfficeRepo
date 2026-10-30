# Modbus register map — Unit ID 1

Protocol addresses below are zero-based. The familiar 1xxxx/3xxxx/4xxxx references are shown for HMI entry.

| Function | PDU address | HMI reference | Quantity | Data |
|---|---:|---:|---:|---|
| FC01 Read Coils | 0 | 00001 | 16 | EL2409 DO state |
| FC02 Read Discrete Inputs | 0 | 10001 | 16 | EL1409 DI state |
| FC03 Read Holding Registers | 0 | 40001 | 4 | EL4074 AO command readback, signed int16 |
| FC04 Read Input Registers | 0 | 30001 | 4 | EL3074 raw signed value |
| FC04 Read Input Registers | 1024 | 31025 | 4 | EL3074 engineering value; default signed mV |
| FC04 Read Input Registers | 2048 | 32049 | 4 | EL3074 status |
| FC04 Read Input Registers | 3072 | 33073 | 4 | EL4074 status |
| FC05 / FC15 | 0..15 | 00001..00016 | 16 | EL2409 write |
| FC06 / FC16 / FC23 | 0..3 | 40001..40004 | 4 | EL4074 write |

Default AO current scaling is signed `0.01 mA/count`. Therefore `132` means `1.320 mA`; the RHEL EtherNet/IP encoder writes `0.00132 A` as REAL32. Values outside `-2000..+2000` are rejected in bipolar +/-20 mA mode.

Unit ID 2 is not part of this release.
