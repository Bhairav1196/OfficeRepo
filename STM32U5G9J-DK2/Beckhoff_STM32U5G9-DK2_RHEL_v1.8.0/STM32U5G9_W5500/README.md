# LADSL // SWAN Fin Stabilizer HMI — v2.0.0-alpha.9.0

This STM32U5G9-DK2, W5500, ThreadX and TouchGFX 4.26.1 project is the single-coupler companion for RHEL gateway v1.8.0. The Modbus process and scaling contract is unchanged: Unit ID 1 provides 16 DI, 16 DO, 4 AI and 4 AO.

## Dark maritime UI

The 800×480 interface is one efficient state-driven root screen with six operator pages. This avoids allocating six complete widget trees and keeps page changes deterministic on the MCU.

| Page | Purpose |
|---|---|
| I/O | Simultaneous 4×4 DI and DO LED matrices plus four AI and four AO vertical telemetry bars |
| Motor | Pump/fan state, contactor, overload, current, run-hour and interlock framework |
| Sensors | Bilge, tank, exhaust, ambient and vessel auxiliary sensor framework |
| Fin LCP | Port/starboard angle, hydraulic pressure, oil temperature, mode and roll framework |
| MCR | Main-engine, gearbox, generator, bus, load-sharing and alarm framework |
| Bridge | Heading, SOG, roll/pitch, system flags and master-alarm framework |

The title is `LADSL // SWAN Fin Stabilizer`. The visual system uses deep matte black `#0B0F19`, glassy navy panels, tactical cyan `#00E5FF`, white typography, green active I/O, safety amber and critical red `#FF3B30`. A slim 40-pixel layered header and regenerated rounded assets provide a restrained aero/glass effect. Roboto Regular/Medium sizes are increased for 800×480 visibility.

Only the I/O page is connected to the present Beckhoff data contract. Other pages intentionally show `---` or `NO DATA` until their corresponding services are added; the GUI never fabricates live machinery or navigation values.

## I/O and safety behavior

- Poll cycle: DI → DO → AI → AO, always Unit ID 1.
- AI: FC04 address 1024, four bipolar ±10 V channels.
- AO: FC03 address 0, four bipolar ±20 mA channels.
- AO values remain available through the service API; the status page does not expose accidental touch writes.
- Fractional formatting retains `%01u`, `%02u` and the proven `%03u` fix.

## Independent watchdog

The STM32 IWDG starts before ThreadX with an approximately two-second timeout. The supervisor refreshes it only after both the Modbus/W5500 transport task and diagnostics task have advanced. A network outage does not count as a task failure, but task starvation, deadlock, termination or failed task creation allows the IWDG to reset the MCU.

## Build

1. Run `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` on the Windows workstation. It locates TouchGFX 4.26.1 tools, uses an installed Roboto family when available, or downloads the official Google Fonts package and extracts `Roboto-Regular.ttf` and `Roboto-Medium.ttf`. An offline workstation may copy those two files into `TouchGFX\assets\fonts` before running the helper.
2. Open `TouchGFX/STM32U5G9_DK2_W5500_TouchGFX_ModbusTCP_Master.touchgfx` and select **Generate Code**.
3. Import `STM32CubeIDE` as an existing project in STM32CubeIDE 2.1.1.
4. Clean-build, flash and verify display, touch, W5500, RHEL gateway, EK9500 and watchdog-reset behavior.

Do not hand-edit `TouchGFX/generated`; Designer regeneration owns that directory. Application logic is under `Application/` and `TouchGFX/gui/`.

## Host verification

```bash
bash tests/run_release_verification.sh
```

Host checks cover configuration/scaling, Modbus, W5500 mocks, simultaneous AI/AO MVC data, all six page definitions, Roboto typography, IWDG integration and strict application C compilation. Target ARM build and hardware-in-the-loop acceptance remain required.
