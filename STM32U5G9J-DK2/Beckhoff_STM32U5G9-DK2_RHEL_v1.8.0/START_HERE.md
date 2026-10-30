# Start here — single Beckhoff I/O station

This package contains a matched RHEL gateway and STM32U5G9/W5500 TouchGFX client for one EK9500 station:

`EK9500 (192.168.1.1) -> EL1409 -> EL2409 -> EL3074 -> EL4074`

The station is Modbus Unit ID 1 and provides 16 DI, 16 DO, 4 AI and 4 AO.

## Quick start

1. Confirm the terminal order above and configure the EK9500 for a 30-byte input assembly and 22-byte output assembly.
2. In `RHEL/`, run `./scripts/build.sh`, then `./scripts/run.sh`.
3. In `STM32U5G9_W5500/`, run `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat`, regenerate with TouchGFX 4.26.1, then clean-build in STM32CubeIDE.
4. Commission every channel using `RHEL/docs/COMMISSIONING_GUIDE.md`.

The GUI shows all digital channels and lets the operator switch the analog deck between AI and AO. AO editing is available only in the AO view.

See `README.md` for the release overview and `VALIDATION_REPORT.md` for verified scope and remaining hardware checks.
