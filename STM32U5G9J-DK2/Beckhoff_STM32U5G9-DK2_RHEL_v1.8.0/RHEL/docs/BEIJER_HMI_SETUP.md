# Beijer iX HMI setup — single station

Configure one Modbus TCP controller at the RHEL gateway address, normally TCP 502, using Unit ID 1. Add the HMI source address to `config/masters.conf` with `scripts/masterctl.sh`.

| Signal | Zero-based PDU address | Traditional reference |
|---|---:|---|
| DO1..DO16 | Coils 0..15 | 00001..00016 |
| DI1..DI16 | Discrete inputs 0..15 | 10001..10016 |
| AI1..AI4 raw | Input registers 0..3 | 30001..30004 |
| AI1..AI4 engineering | Input registers 1024..1027 | 31025..31028 |
| AI status | Input registers 2048..2051 | 32049..32052 |
| AO1..AO4 command/readback | Holding registers 0..3 | 40001..40004 |
| AO status | Input registers 3072..3075 | 33073..33076 |

AO tags are signed 16-bit integers at 0.01 mA/count. For example, `132` means 1.320 mA and becomes 0.00132 A in the EL4074 REAL32 PDO. Confirm the Beijer driver's address-base convention and test positive, negative and zero commands with a meter before connecting operational loads.
