# Architecture — single Beckhoff station

```mermaid
flowchart LR
    S["STM32U5G9 + W5500"] -->|"Modbus TCP, Unit 1"| G["RHEL gateway"]
    H["Beijer / ModScan"] -->|"Modbus TCP, Unit 1"| G
    G -->|"EtherNet/IP Class 1"| E["EK9500 + DI + DO + AI + AO"]
```

The gateway owns one process image and one EtherNet/IP worker. TCP 1502 and optional TCP 502 are listener alternatives, not separate couplers. Master ACL and per-client rate limits apply to both listeners.
