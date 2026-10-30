## [2.0.0-alpha.9.0] - 2026-09-11

- Rebuilt TouchGFX as a six-page, state-driven dark maritime framework titled `LADSL // SWAN Fin Stabilizer`.
- Page 1 now shows 16 DI, 16 DO, four AI and four AO simultaneously using LED matrices and vertical analog bars.
- Added Motor, Sensors, Fin LCP, MCR and Bridge pages with explicit no-data states pending their service integrations.
- Switched to larger Roboto typography, a slim layered header, dark glass panels and rounded dark assets.
- Added an approximately two-second STM32 IWDG refreshed only when transport and diagnostics task heartbeats both advance.
- Preserved the single-coupler Modbus contract and `%03u` fractional formatting fix.

## [2.0.0-alpha.7.3] - 2026-09-10

### Fixed
- Removed `cmd.exe` from the TouchGFX simulator asset-build wrapper. Under TouchGFX GNU Make/MSYS, the previous Windows check could start an interactive command shell and leave Designer stuck at `Building`.
- Asset prerequisites are now validated by GNU Make itself using parse-time `wildcard` / `error` checks; no subprocess is started during Generate Assets.
- Corrected `APP_VERSION_PRERELEASE` to `alpha.7.3`.

### Compatibility
- No change to W5500, ThreadX, Modbus TCP, I/O mapping, TouchGFX MVC logic, or the matched RHEL v1.5.1 contract.


## [2.0.0-alpha.7.3] - 2026-09-10

- Removed recursive STM32Cube repository scanning from TouchGFX Generate/Build.
- Asset generation now performs only fast local existence checks.
- `PREPARE_TOUCHGFX_ENVIRONMENT.bat` is a one-time explicit preparation step.
- Tool discovery uses only deterministic TouchGFX 4.26.1 paths, environment overrides, and a bounded CLI-parent lookup.
- Prevents TouchGFX Designer appearing indefinitely stuck at `Building`.
- Communication, W5500, Modbus, ThreadX, HMI and matched RHEL v1.5.1 behavior are unchanged.
# Changelog

This project follows Semantic Versioning.

## [2.0.0-alpha.7.3] - 2026-09-10

### Fixed
- Restored the missing TouchGFX 4.26.1 Windows asset-tool integration that caused Designer `Generate Assets` to fail before compilation.
- Added `Tools\PREPARE_TOUCHGFX_TOOLS.bat` + `prepare_touchgfx_tools.ps1` to locate an official TouchGFX/X-CUBE-TOUCHGFX 4.26.1 installation, verify `TOUCHGFX_VERSION=42601`, and populate project-local `imageconvert`/`fontconvert` Windows build directories.
- Added `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` to prepare both vendor asset converters and the locally licensed Segoe UI/Roboto source font.
- Made the user-owned simulator Makefile run the environment preparation before `assets` on Windows, keeping generated Makefiles untouched and regeneration-safe.
- The source package continues to exclude ST converter binaries and font binaries.

### Preserved
- No Modbus mapping, W5500 transport, RHEL v1.5.1 compatibility, ThreadX tasking, I/O scaling, or HMI behavior changes.

### Verification
- Added release checks for the self-healing asset-tool prerequisite, tool-source version validation, environment helper, and packaging policy.
- Existing project-contract, host Modbus/W5500/configuration, TouchGFX binding and C/C++ boundary tests remain required.

## [2.0.0-alpha.7] - 2026-09-10

### Matched RHEL v1.5.1 release
- Pairs the STM32U5G9-DK2/W5500 master with RHEL compatibility ingress v1.5.1 on `192.168.1.7:502`.
- Fixes the expected master contract at one persistent connection and 500 requests/s.
- Treats TCP active-open refusal (`SYN -> RST,ACK`) as server unavailability; connect failures no longer trigger W5500 hardware reinitialization.
- `fn_W5500_Connect()` guards an already `ESTABLISHED` Socket 0 before any local normalization.
- Failed/new active opens use local socket abort/`CLOSE`, not a graceful `DISCON` against a live session.
- Valid Modbus exception responses no longer increment protocol-error reconnect counters.
- Reconnect backoff changed to 1000 ms.
- Added connect-attempt, connect-failure, consecutive-connect-failure and Modbus-exception diagnostics.
- Corrected TouchGFX numeric formatting to avoid `%lu` length modifiers in dynamic AI/AO/uptime text.
- Removed obsolete RTOS socket-close helper after centralizing connection ownership.

### Verification
- Project contract verifier: 175 checks PASS.
- TouchGFX C/C++ build-boundary checks PASS.
- Host analog/configuration/Modbus/W5500 tests PASS.
- Strict release verification PASS.
- ARM target link, flash and physical HIL remain commissioning gates.

## [2.0.0-alpha.6.2] - 2026-09-10
- Debounced transient non-`ESTABLISHED` `Sn_SR` samples before reconnect.
- Suppressed active TCP disconnect decisions when PHYCFGR disagrees with a positively established Socket 0.
- Converted repeated PHY/socket-status SPI anomalies to diagnostics rather than active disconnect triggers.
- Added socket/PHY disagreement and suppressed-reconnect diagnostic counters.

## [2.0.0-alpha.6.1] - 2026-09-10
- Fixed TouchGFX Program-and-Run-Target `hmi_service.h` include/link integration.
- Added the complete application-owned module set through regeneration-safe `TouchGFX/config/gcc/app.mk`.

## [2.0.0-alpha.6] - 2026-09-10
- Removed socket teardown from the Modbus protocol layer.
- Made the ThreadX transport task the connection-lifecycle owner.
- Short-circuited polling after the first failed exchange.
- Hardened W5500 volatile 16-bit counter reads and added explicit disconnect/transaction diagnostics.

## [2.0.0-alpha.5] - 2026-09-10
- Fixed two-unit topology: Unit 1 = 16 DI + 16 DO + 4 AI; Unit 2 = 16 DI + 16 DO + 4 AO.
- Added unit selection, 16 DI/DO channels, four analog cards, metallic-light rounded HMI and Segoe UI/Roboto-local font preparation contract.

## [2.0.0-alpha.4] - 2026-09-10

### Fixed
- Corrected the TouchGFX 4.26.1 simulator C/C++ compiler boundary. Application `.c` files are compiled with `gcc` while TouchGFX `.cpp` files remain on `g++`.
- Added a C-only `c_compiler_options_local` override in the user-owned simulator wrapper so C compilation does not receive the C++-only `-Wno-overloaded-virtual` option under `-Werror`; the TouchGFX C++ warning policy remains unchanged.
- Preserved C11 designated initializers and `_Static_assert` in the typed I/O configuration instead of weakening the configuration model or requiring C++20.

### Verification
- Added a regression check that dry-runs the TouchGFX simulator compile rule and verifies `io_config.c` is compiled by `gcc` without `overloaded-virtual`, while `Screen1View.cpp` remains on `g++` with its C++ warning policy.
- Re-ran structural verification, host unit tests, strict C11 compilation, TouchGFX MVC/simulator-backend tests, package hygiene, and ZIP integrity checks.

## [2.0.0-alpha.3] - 2026-09-10

### Fixed
- TouchGFX 4.26 simulator was invoking its generated `c_compiler` as `g++`, causing application `.c` files to be parsed as C++ and rejecting C11 designated initializers and `_Static_assert`.
- The user-owned `TouchGFX/simulator/gcc/Makefile` now overrides only the nested simulator C compiler with `c_compiler=gcc`; generated TouchGFX Makefiles remain untouched.
- Added a release-contract check that fails verification if the simulator C compiler override is removed.

### Preserved
- `io_config.c` remains clean C11 with readable designated initializers; no positional-initializer downgrade and no C++20 dependency were introduced.
- CubeIDE target code, TouchGFX C++ sources, configuration schema, Modbus mapping and W5500 transport behavior are unchanged by this simulator-boundary correction.

## [2.0.0-alpha.2] - 2026-09-09

### Added
- Consolidated modular `Application/` architecture separated from CubeMX/ST/TouchGFX-generated code.
- Central typed I/O configuration with enable/disable, names, Modbus mapping, datatype, HMI visibility and stable signal IDs.
- Current modes: ±20 mA, 0–20 mA, 4–20 mA and custom current.
- Voltage modes: ±10 V, 0–10 V and custom voltage.
- Electrical hardware, electrical operating, electrical mapping and process operating ranges.
- Linear and piecewise-linear electrical ↔ process mapping.
- AO authoritative range rejection/clamp policy; AI out-of-range quality reporting.
- Duplicate mapped-register/bit validation.
- Configuration-driven TouchGFX labels, process units, precision and signed values.
- Decluttered 92-component minimal operator Screen1.
- TCP status and continuous communication uptime display.
- W5500 PHY read-error distinction and debounced link/socket recovery diagnostics.
- Multiple application ThreadX tasks with one transport owner.
- CubeIDE source/header/include synchronization tool.
- Doxygen configuration and generation helper.
- SemVer consistency checks and v2 host regression suite.

### Changed
- Application-owned W5500, Modbus, IO, diagnostics and RTOS code moved out of `Core/`.
- HMI now consumes process-domain values instead of raw/status register presentation.
- `s_snapshot` expanded with PHY/socket/reconnect/task diagnostics.
- All application-owned modules standardized on `fn_` function naming and Doxygen author metadata.

### Fixed
- Removed stale v1 CubeIDE source links and include paths.
- Prevented PHY SPI read failure from being treated immediately as physical Ethernet link-down.
- Migrated stale v1 host tests to v2 APIs and source paths.

### Known prerelease gates
- TouchGFX Designer Generate Code must be run on the target workstation.
- STM32CubeIDE ARM target build and hardware-in-the-loop testing remain required before RC promotion.

## [2.0.0-alpha.1] - 2026-09-09
- Initial v2 architectural refactor work-in-progress.

## [1.2.3] - 2026-09-09
- Last clean v1.x integrated baseline with working CubeIDE/TouchGFX simulator integration.
