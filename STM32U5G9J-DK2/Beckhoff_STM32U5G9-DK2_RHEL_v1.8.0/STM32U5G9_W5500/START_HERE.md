# STM32 start here

Target: STM32U5G9J-DK2, W5500 SPI Ethernet, ThreadX and TouchGFX 4.26.1.

1. Run `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` once after extraction.
2. Open the `.touchgfx` project and generate code with TouchGFX 4.26.1.
3. Open the existing STM32CubeIDE project and perform a clean build.
4. Connect the W5500 network to the RHEL gateway at `192.168.1.7:502`.

Firmware v2.0.0-alpha.9.0 polls only Modbus Unit ID 1. See `README.md` for the mapping and HMI behavior.
