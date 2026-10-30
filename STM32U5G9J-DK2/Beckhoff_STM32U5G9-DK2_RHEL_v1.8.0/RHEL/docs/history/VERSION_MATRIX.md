# v1.2.x → v1.5.0 Version Matrix

| Version | Primary change | Verification baseline |
|---|---|---|
| v1.2.0 | Dual-coupler multi-master gateway, Unit-ID routing, runtime ACL | 5/5 host tests |
| v1.2.1 | Configuration test corrected for mutable ACL | inherited baseline |
| v1.2.2 | Shared listeners on TCP 1502 and 502 | 6/6 host tests |
| v1.2.3 | Central signed AO conversion, intermediate scaling interpretation | conversion tests |
| v1.2.4 | EL4074 REAL32 corrected to SI amperes; `132 -> 0.00132 A` | scaling patch validation |
| v1.3.0 | Consolidated code/docs/Doxygen/scripts; dedicated AO regression and monitor | 7/7 standard, analyzer and sanitizer tests |
| v1.4.0 | STM32U5/W5100 master profile, optional per-master request rate, contract tool/docs | 9/9 standard plus contract self-test |
| v1.5.0 | STM32F407/LAN8720 implemented master baseline; current ACL/contract tooling | 9/9 standard plus embedded-contract self-test |

The authoritative current behaviour is v1.5.0. v1.5.0 preserves the v1.4.0
Modbus register/wire contract and adds a backward-compatible fourth ACL field:

```text
name|IPv4|max_connections[|max_requests_per_second]
```
