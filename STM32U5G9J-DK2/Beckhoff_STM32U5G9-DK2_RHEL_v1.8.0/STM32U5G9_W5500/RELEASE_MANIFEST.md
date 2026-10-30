# Release manifest — v2.0.0-alpha.9.0

| Item | Value |
|---|---|
| Target | STM32U5G9J-DK2 / STM32U5G9ZJT6Q |
| Ethernet / RTOS | W5500 SPI1 / Azure RTOS ThreadX |
| GUI | TouchGFX 4.26.1, 800×480, six state-driven pages |
| Typography | Roboto Regular and Medium, locally prepared |
| Gateway | RHEL v1.8.0 at 192.168.1.7:502 |
| Modbus topology | Unit ID 1; 16 DI, 16 DO, 4 AI, 4 AO |
| Watchdog | IWDG ~2 s; transport + diagnostics health gated |

The source package excludes font binaries and generated host build products. Run TouchGFX generation, CubeIDE ARM build, flash and physical acceptance before release to a vessel test environment.
