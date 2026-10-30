# Validation report — single-coupler release

Date: 2026-09-11. Validation was performed on Linux x86-64 without physical Beckhoff or STM32 hardware.

| Check | Result |
|---|---|
| RHEL strict C11 build and regression programs | PASS |
| RHEL executable EtherNet/IP/Modbus loopback integration | PASS — 12 groups |
| One combined route, 30-byte input / 22-byte output PDO | PASS |
| AO conversion, register 132 → 1.320 mA → 0.00132 A | PASS |
| 3,150 paced polls over 10 seconds | PASS — zero exceptions |
| Four simultaneous masters, 2,000 requests | PASS — zero exceptions |
| STM32 configuration/scaling, Modbus and W5500 host tests | PASS |
| TouchGFX MVC and simulator backend host tests | PASS |
| STM32 project contract | PASS — 199 checks |
| TouchGFX dark six-page framework | PASS — 184 Designer components / 120 required bindings |
| Simultaneous DI/DO/AI/AO HMI model | PASS |
| Roboto typography and dark tactical palette contract | PASS |
| IWDG pre-RTOS start and dual-task heartbeat gating | PASS |
| Strict application C compilation and build-rule checks | PASS |

Saved command output is in `validation/`. Build products are intentionally excluded from the source package.

## Remaining acceptance work

Run TouchGFX Designer 4.26.1 code generation, a clean STM32CubeIDE ARM build and flash, then commission the real EK9500 and all four terminal groups. Confirm assembly sizes, electrical modes, safe output behavior, network fault recovery and long-duration operation. These host results are regression evidence, not industrial, maritime, safety or protocol-conformance certification.
