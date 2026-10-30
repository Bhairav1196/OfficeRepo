# STM32U5G9 DK2 + W5500 + ThreadX + TouchGFX Modbus TCP Master

**Firmware:** `v2.0.0-alpha.7.3`  
**Board:** STM32U5G9J-DK2 / STM32U5G9ZJT6Q  

> **alpha.7.3 TouchGFX build rule:** Run `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` once after extraction. Generate/Build performs only GNU Make parse-time file checks; it never starts `cmd.exe`, PowerShell, or recursive discovery, so Designer cannot stall at an interactive command prompt.

**Ethernet:** WIZnet W5500 on SPI1  
**RTOS:** Azure RTOS ThreadX  
**HMI:** TouchGFX 4.26.1, 800x480  
**Protocol:** Modbus TCP Master to the RHEL gateway  
**Author:** Prathamesh Chipkar

> `2.0.0-alpha.7.3` is a Semantic Versioning prerelease. The source/configuration, host regressions and project-contract checks pass in this package. Final acceptance still requires TouchGFX Designer regeneration, STM32CubeIDE ARM build, flashing, and hardware-in-the-loop verification on the STM32U5G9-DK2 + W5500 + RHEL + two EK9500 couplers.

## Alpha.7.1 TouchGFX asset-tool repair

This maintenance release fixes the Windows TouchGFX 4.26.1 generation failure:

```text
process_begin: CreateProcess(NULL, ../Middlewares/ST/touchgfx/framework/tools/imageconvert/build/win/imageconvert.out ...) failed
make (e=2): The system cannot find the file specified.
```

The source package intentionally does **not** redistribute ST's host converter binaries. Instead, `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` checks deterministic official TouchGFX/X-CUBE-TOUCHGFX **4.26.1** locations (or `TOUCHGFX_TOOL_SOURCE`), verifies `TOUCHGFX_VERSION=42601`, and copies the local Windows `imageconvert`/`fontconvert` build directories into the project paths expected by the generated Makefiles. It then prepares the locally licensed Segoe UI/Roboto source font.

The user-owned `TouchGFX/simulator/gcc/Makefile` also makes this preparation an `assets` prerequisite on Windows. Therefore Designer **Generate Code** self-checks/repairs the local converter paths before invoking generated asset rules. If no compatible official 4.26.1 installation can be found, generation stops with a precise remediation message instead of the opaque `CreateProcess` failure.

## Alpha.7 matched RHEL v1.5.1 communication release

This release is paired with **RHEL gateway compatibility release v1.5.1** and
keeps the established Unit-ID/register/scaling contract unchanged. The matched
network baseline is:

```text
STM32/W5500 : 192.168.1.60/24
RHEL ingress: 192.168.1.7:502
TCP sessions: 1 persistent connection
RHEL ceiling: 500 requests/s
Poll period : 20 ms
```

Alpha.7 specifically hardens the failure modes observed on the live network:

- a RHEL `SYN -> RST,ACK` refusal is treated as **server unavailable**, not a
  W5500 hardware fault;
- repeated connect failures do not reset a W5500 that still passes SPI/version/
  PHY checks;
- failed active-open attempts use local socket `CLOSE` normalization and do not
  issue a graceful `DISCON` against an established session;
- `fn_W5500_Connect()` samples Socket 0 before opening a new connection and
  returns success immediately if `ESTABLISHED` is observed;
- valid Modbus exception replies are counted separately and do not drive TCP
  reconnect policy;
- reconnect backoff is 1000 ms, with a fresh bounded source port on each real
  new active-open attempt;
- TouchGFX dynamic numeric formatting avoids `%lu` length modifiers that can
  leak formatting text into AI/AO widgets;
- the alpha.6/alpha.6.2 persistent-session debounce and the alpha.6.1
  regeneration-safe TouchGFX build integration remain retained.

The matching RHEL ingress ACL entry is:

```text
STM32U5G9DK2_W5500|192.168.1.60|1|500
```

A healthy commissioned run should establish one TCP session on RHEL port 502
and keep the same connection open while the session-duration counter increases.
A temporary backend outage may return Modbus exception `0x0B`; it must not by
itself cause this firmware to reset the W5500.

## 1. Fixed Beckhoff topology

This release explicitly models **two distinct Modbus logical units / Beckhoff couplers**. They are not treated as interchangeable generic I/O banks.

| Logical Unit | Physical role | DI | DO | AI | AO |
|---|---|---:|---:|---:|---:|
| **Unit 1** | DI + DO + AI coupler | **16** | **16** | **4** | 0 |
| **Unit 2** | DI + DO + AO coupler | **16** | **16** | 0 | **4** |

Total configured signal capacity in the default profile is therefore **72 signals**: 32 DI + 32 DO + 4 AI + 4 AO.

### Proven RHEL Modbus mapping

All addresses below are **zero-based Modbus PDU addresses**.

| Unit | I/O | FC | Address | Qty | Meaning |
|---|---|---:|---:|---:|---|
| 1 | DO | 01 / 05 / 15 | 0 | 16 | EL2409 DO1..DO16 |
| 1 | DI | 02 | 0 | 16 | EL1409 DI1..DI16 |
| 1 | AI raw | 04 | 0 | 4 | EL3074 signed raw values |
| 1 | AI engineering | 04 | 1024 | 4 | EL3074 engineering registers, currently mV representation |
| 1 | AI status | 04 | 2048 | 4 | EL3074 status; diagnostics only |
| 2 | DO | 01 / 05 / 15 | 0 | 16 | DO1..DO16 |
| 2 | DI | 02 | 0 | 16 | DI1..DI16 |
| 2 | AO command/readback | 03 / 06 | 0 | 4 | EL4074 signed `int16_t`, 0.01 mA/count by default |
| 2 | AO status | 04 | 3072 | 4 | EL4074 status; diagnostics only |

The operator HMI does **not** show raw/status registers on the main screen. They remain available through diagnostics and CubeIDE.

## 2. Architecture and ownership

The central maintenance rule is:

> **CubeMX and TouchGFX must be able to regenerate the files they own without overwriting application business logic.**

```text
Project
|
+-- Core/                       CubeMX-owned initialization / USER CODE hooks only
+-- Drivers/                    ST-owned
+-- Middlewares/                ST/Azure-owned
+-- AZURE_RTOS/                 CubeMX/ST-owned
+-- TouchGFX/
|   +-- generated/              TouchGFX-owned - do not hand edit
|   +-- target/                 TouchGFX target integration
|   +-- gui/                    application-owned GUI Presenter/View/Model code
|   +-- simulator/              application-owned simulator backend + generated simulator
|
+-- Application/                application-owned firmware
|   +-- Common/
|   +-- Config/
|   +-- Drivers/
|   +-- Interfaces/
|   +-- RTOS/
|   +-- Services/
|
+-- Tests/                      host regression tests
+-- Tools/                      synchronization / verification helpers
+-- Docs/                       project documentation
```

Application logic should not be added directly to generated files except for the minimal protected CubeMX hook required to enter the application-owned ThreadX bootstrap.

## 3. Code startup flow

```text
Power / Reset
    |
    v
STM32 startup
    |
    v
HAL_Init()
    |
    v
SystemClock_Config()
    |
    v
CubeMX peripheral initialization
(GPIO / SPI1 / display / etc.)
    |
    v
ThreadX initialization
    |
    v
App_ThreadX_Init()
    |
    +-- protected USER CODE hook
    |        |
    |        v
    |   fn_AppRtos_Create()
    |        |
    |        +-- Transport Task
    |        +-- Diagnostics Task
    |        +-- Supervisor Task
    |
    +-- ST/TouchGFX task initialization
```

## 4. Runtime communication flow

### Beckhoff input/read path

```text
EK9500 Unit 1 / Unit 2
        |
        | EtherNet/IP handled by RHEL gateway
        v
RHEL Modbus TCP Server
        |
        | TCP / Modbus
        v
W5500
        |
        | SPI1
        v
W5500 Driver
        |
        v
Modbus TCP Service
        |
        v
IO Service
        |
        +-- register value
        +-- electrical conversion (mA / V)
        +-- range / quality check
        +-- optional process conversion (deg / bar / RPM / ...)
        |
        v
Named engineering signal model
        |
        v
HMI Service
        |
        v
TouchGFX Model -> Presenter -> Screen1View
```

### DO/AO operator command path

```text
TouchGFX operator command
        |
        v
Screen1View
        |
        v
Presenter / Model
        |
        v
HMI Service
        |
        +-- validate enabled/writable
        +-- validate process limit
        +-- process -> electrical conversion
        +-- validate electrical operating/hardware limit
        +-- electrical -> register conversion
        |
        v
ThreadX command queue
        |
        v
Transport Task
        |
        +-- DO -> FC05
        +-- AO -> FC06
        |
        v
W5500 -> RHEL -> EK9500
```

AO writes are **rejected** when they exceed the configured limits unless a channel is deliberately configured with the clamp policy.

## 5. ThreadX ownership model

Only the **Transport Task owns W5500, TCP and Modbus transaction execution**. Other tasks must not directly compete for SPI/socket access.

| Task | Default priority | Default stack | Responsibility |
|---|---:|---:|---|
| Transport | 6 | 4096 B | W5500 state machine, TCP session, cyclic Modbus reads, queued writes |
| Diagnostics | 14 | 2048 B | communication/PHY/socket/runtime observations |
| Supervisor | 15 | 2048 B | application supervision and service-health cadence |
| TouchGFX | ST/CubeMX | ST/CubeMX | operator HMI |

Thread priorities and stack ceilings are configured in `Application/Config/Inc/app_config.h`. Final stack margins should be confirmed by measuring ThreadX high-water usage on target hardware.

## 6. Authoritative configuration mechanism

The project deliberately uses a **hybrid configuration strategy**:

- macros: feature presence, capacities, RTOS sizing, fixed build policy;
- typed `const` structures: user-editable I/O mapping, name, mode, range, scaling and HMI settings.

The main commissioning file is:

```text
Application/Config/Src/io_config.c
```

Do not create a second copy of I/O names/units inside TouchGFX. The HMI consumes the same configuration table.

### Per-signal configurable properties

Each signal can define:

- stable signal ID;
- name;
- Unit ID;
- DI/DO/AI/AO type;
- enable/disable;
- read/write permission;
- Modbus function/address/bit index;
- datatype;
- analog current/voltage mode;
- register-to-electrical scale and offset;
- electrical hardware limits;
- electrical operating limits;
- electrical calibration/map range;
- optional electrical-to-process mapping;
- process operating limits;
- process unit;
- decimal precision;
- HMI step for writable AO;
- write range policy;
- HMI visibility and unit-specific HMI slot.

### Enable / disable

Use the signal's typed configuration, not scattered `#ifdef`s:

```c
.b_Enabled = true,
```

or:

```c
.b_Enabled = false,
```

A disabled output is not allowed to become writable. The HMI can represent the configured signal as `DISABLED` without losing its identity/name.

## 7. Analog electrical modes

Supported standard modes are:

| Mode | Electrical range | Derived unit |
|---|---:|---|
| `E_IO_ANALOG_CURRENT_BIPOLAR_20MA` | -20.000 .. +20.000 | mA |
| `E_IO_ANALOG_CURRENT_0_20MA` | 0.000 .. +20.000 | mA |
| `E_IO_ANALOG_CURRENT_4_20MA` | +4.000 .. +20.000 | mA |
| `E_IO_ANALOG_VOLTAGE_BIPOLAR_10V` | -10.000 .. +10.000 | V |
| `E_IO_ANALOG_VOLTAGE_0_10V` | 0.000 .. +10.000 | V |
| custom current | configured | mA |
| custom voltage | configured | V |

The electrical unit is derived from the mode; it is not separately hard-coded in the HMI.

### Default Unit 1 AI configuration

Unit 1 AI1..AI4 default to **bipolar ±10 V**, using engineering register addresses 1024..1027 and 0.001 V/count interpretation of the mV register representation.

Typical HMI output:

```text
UNIT 1 AI 01
+7.500 V
```

### Default Unit 2 AO configuration

Unit 2 AO1..AO4 default to **bipolar ±20 mA**, 0.01 mA/count:

```text
-2000 -> -20.000 mA
 -500 ->  -5.000 mA
    0 ->  +0.000 mA
  132 ->  +1.320 mA
  899 ->  +8.990 mA
 2000 -> +20.000 mA
```

Attempts beyond ±20 mA are rejected in the default configuration.

## 8. Electrical-to-process mapping

The electrical value is an intermediate domain. A channel can optionally expose a real process quantity.

```text
REGISTER DOMAIN
    2000
      |
      v
ELECTRICAL DOMAIN
    +20.000 mA
      |
      v
PROCESS DOMAIN
    +35.000 deg
```

Example: PORT FIN ANGLE where ±20 mA represents ±35 degrees.

```c
.e_AnalogMode = E_IO_ANALOG_CURRENT_BIPOLAR_20MA,
.st_ElectricalHardwareRange = {-20.000F, +20.000F},
.st_ElectricalOperatingRange = {-20.000F, +20.000F},
.st_ElectricalMapRange = {-20.000F, +20.000F},
.e_MappingType = E_IO_MAPPING_LINEAR,
.st_ProcessMapRange = {-35.000F, +35.000F},
.st_ProcessOperatingRange = {-35.000F, +35.000F},
.pc_ProcessUnit = "deg",
.ui8_DecimalPlaces = 3U,
```

The HMI then shows:

```text
PORT FIN ANGLE
+17.500 deg
```

for a +10.000 mA electrical input.

If `E_IO_MAPPING_NONE` is used, TouchGFX displays the electrical quantity (`mA` or `V`) directly. Linear, reversed/asymmetric and piecewise-linear calibration are supported by the I/O service.

## 9. Range and quality model

Analog limits are intentionally separated:

```text
Electrical hardware range
        |
Electrical operating range
        |
Electrical mapping/calibration range
        |
Process mapping range
        |
Process operating range
```

This permits, for example, hardware capability ±20 mA but an application operating limit of ±18 mA without changing the calibration law.

For AI, an out-of-range measurement is preserved and marked with `E_IO_QUALITY_OUT_OF_RANGE`. For AO, an out-of-range command is rejected before the Modbus write by default.

Startup `fn_IOConfig_ValidateAll()` also checks unit topology, signal IDs, access, mode/range consistency, duplicates, HMI slots and process mapping validity.

## 10. TouchGFX operator HMI

The main screen is intentionally an **operator view rather than a protocol debugger**.

### Theme

- metallic-light / brushed-silver visual direction;
- light graphite/silver panels;
- restrained active/warning/fault colors;
- rounded Unit-selector and AO adjustment buttons;
- no raw/status-register clutter on the operator screen;
- configuration-driven signal names and units;
- analog values displayed to up to three decimal places with the correct sign.

### Font

The descriptor uses `segoeui.ttf` as the project font name. **No font binary is shipped in this ZIP.**

Recommended one-time preparation before opening Designer:

```text
Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat
```

This prepares both the official TouchGFX 4.26.1 host converters and the local font. `PREPARE_TOUCHGFX_FONT.bat` remains available if only the font needs to be refreshed. The font helper prefers a locally available **Segoe UI** font. If unavailable, it can use a locally installed **Roboto Regular** as the source while keeping the TouchGFX project filename stable. The user is responsible for the locally installed font/licensing environment.

### Unit selection

The 800x480 screen uses rounded **UNIT 1 / UNIT 2** selectors so the full 72-signal topology stays readable.

**UNIT 1 view**

```text
16 DI + 16 DO + 4 AI
```

**UNIT 2 view**

```text
16 DI + 16 DO + 4 AO
```

Each selected unit shows all sixteen DI and all sixteen DO in two compact columns, plus four analog cards. AO +/- controls appear only for enabled/writable Unit 2 AO channels.

The main header retains only:

```text
TCP: CONNECTED / DISCONNECTED
COMM UPTIME: n seconds
```

Communication uptime means the duration of the current continuous healthy communication interval and resets when the connection is lost.

## 11. TouchGFX data ownership

TouchGFX does not know Modbus addresses or W5500 registers.

```text
io_config.c
    |
IO Service
    |
HMI Service
    |
TouchGFX Model
    |
Presenter
    |
Screen1View
```

`Screen1View` receives names, values, units, quality, writable state and precision through `S_HMI_SignalView`.

This prevents the GUI from developing a second mapping database that could drift away from firmware configuration.

## 12. W5500 / TCP behavior

The W5500 uses SPI1:

| Signal | STM32 pin |
|---|---|
| SCK | PA5 |
| MISO | PA6 |
| MOSI | PB5 |
| CS | PB0 |
| RESETn | PB1 |

SPI policy: Mode 0, 8-bit, MSB first, software NSS, prescaler /8.

The application uses one persistent Modbus TCP connection under normal operation. Socket 0 is the communication socket. Link handling distinguishes a genuine PHY-link-down condition from a W5500/SPI PHY-register read failure and debounces destructive recovery.

The firmware's nominal read load remains approximately:

```text
6 fast Modbus reads x 50 cycles/s = 300 requests/s
3 diagnostic reads x 5 cycles/s   =  15 requests/s
                                     ----------------
                                     ~315 requests/s
```

Occasional DO/AO writes are additional. For the RHEL ACL, a practical per-STM32 setting is **max 2 TCP connections** and approximately **400 requests/s** (or 450 during commissioning if desired) while retaining the gateway's established hard ceiling.

## 13. CubeIDE diagnostic snapshot

For target debugging, add the application diagnostic snapshot to **Debug -> Expressions / Live Expressions**. The target maintains:

```c
S_AppDiagnosticSnapshot s_snapshot;
```

Important fields include:

```text
ui16_Unit1DiMask
ui16_Unit1DoMask
ai16_Unit1AiRaw[4]
ai16_Unit1AiEngineeringRegister[4]
aui16_Unit1AiStatus[4]

ui16_Unit2DiMask
ui16_Unit2DoMask
ai16_Unit2AoRegister[4]
aui16_Unit2AoStatus[4]

b_W5500Ready
b_EthernetLinkUp
b_TcpConnected
ui32_CommunicationUptimeSec
ui32_SuccessfulTransactions
ui32_ProtocolErrors
ui32_TransportErrors
ui32_ReconnectCount
ui32_PhyReadErrors
ui32_PhysicalLinkDownEvents
ui32_SocketDisconnectEvents
ui8_LastPhyCfgr
ui8_LastSocketStatus
ui32_TransportCycleCounter
ui32_DiagnosticsCycleCounter
ui32_SupervisorCycleCounter
```

For DI/DO masks, bit 0 is channel 1 and bit 15 is channel 16.

## 14. CubeMX / TouchGFX regeneration contract

Do not hand-edit `TouchGFX/generated/`. It is deliberately allowed to be regenerated by Designer.

Recommended sequence after extracting the project:

```text
1. Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat
2. Open the .touchgfx project
3. TouchGFX Designer -> Generate Code
4. TouchGFX Designer -> Run Simulator
5. Tools\SYNC_CUBEIDE_PROJECT.bat
6. Open STM32CubeIDE
7. Project -> Clean...
8. Build Project
9. Flash / debug
```

If CubeMX is regenerated later, run the project synchronization/verification helpers again. The application hook is intentionally small and protected.

### TouchGFX simulator C/C++ boundary

Application `.c` files are compiled by `gcc` as C; TouchGFX `.cpp` files remain compiled by `g++`. The non-generated simulator wrapper supplies a C-specific warning list so C sources do not inherit the TouchGFX C++-only `overloaded-virtual` option. Generated TouchGFX Makefiles are not patched.

## 15. Network commissioning

The default/example RHEL address is currently:

```text
192.168.1.7:502
```

It is **not an authoritative site address**. Update the real RHEL host in:

```text
Application/Config/Inc/network_config.h
```

Default STM32/W5500 IPv4 configuration is `192.168.1.60/24`.

## 16. Semantic Versioning

Firmware releases follow:

```text
MAJOR.MINOR.PATCH[-prerelease]
```

Examples:

```text
2.0.0-alpha.7.3
2.0.0-beta.1
2.0.0-rc.1
2.0.0
2.0.1
2.1.0
```

Firmware version, Modbus-map version, HMI API version and configuration-schema version are tracked separately where appropriate.

## 17. Doxygen policy

Application-owned C/C++ headers and sources use Doxygen file/API documentation and include:

```text
@file
@brief
@author Prathamesh Chipkar
@date
@version
```

Public APIs additionally document parameters, return values, notes and ownership constraints where relevant. ST, Azure RTOS, HAL and generated TouchGFX code are not relabeled as user-authored code.

## 18. Verification

This source milestone contains automated checks for:

- exact two-unit topology;
- 72 signal definitions;
- all sixteen DI/DO HMI slots per unit;
- Unit 1 four AI only;
- Unit 2 four AO only;
- current/voltage range/scaling;
- AO limit enforcement;
- AI out-of-range quality;
- electrical/process mapping;
- W5500 link-state handling;
- Modbus transaction behavior;
- TouchGFX unit-aware MVC binding;
- TouchGFX simulator backend;
- Segoe UI typography contract / Roboto fallback helper;
- metallic rounded assets/components;
- CubeIDE linked sources and headers;
- simulator C/C++ compiler separation;
- no font binaries in the source package.

Run on a POSIX host:

```text
tests/run_release_verification.sh
```

On Windows, use the provided verification/build helpers as applicable, followed by the actual TouchGFX Designer and CubeIDE build.

## 19. Release status

`v2.0.0-alpha.7.3` is intended as the **dual-Beckhoff-unit + metallic-light HMI milestone**. The next promotion should occur only after:

1. TouchGFX Designer Generate Code succeeds;
2. Run Simulator succeeds;
3. STM32CubeIDE clean ARM build succeeds;
4. target flashes and starts ThreadX/TouchGFX;
5. W5500 link/TCP remains stable;
6. Unit 1 all 16 DI + 16 DO + 4 AI are verified against Beckhoff/RHEL;
7. Unit 2 all 16 DI + 16 DO + 4 AO are verified;
8. AO limit enforcement and UI writes are verified on hardware;
9. configuration-driven current/voltage/process mappings are spot-checked.


### Alpha.6 CubeIDE live expressions

Add `s_snapshot` and inspect at minimum:

```text
s_snapshot.b_TcpConnected
s_snapshot.e_LastDisconnectReason
s_snapshot.ui32_CurrentTcpSessionDurationMs
s_snapshot.ui32_LastTcpSessionDurationMs
s_snapshot.ui32_IntentionalSocketCloseCount
s_snapshot.ui32_ReconnectCount
s_snapshot.ui32_ConsecutiveTransportFailures
s_snapshot.ui32_ConsecutiveProtocolFailures
s_snapshot.ui8_LastFailedUnitId
s_snapshot.ui8_LastFailedFunctionCode
s_snapshot.ui16_LastFailedAddress
s_snapshot.ui16_LastFailedQuantity
s_snapshot.ui16_LastTransactionId
s_snapshot.i32_LastW5500Result
s_snapshot.ui32_W5500StableReadRetries
s_snapshot.ui32_W5500StableReadFailures
```

`e_LastDisconnectReason == E_TCP_DISCONNECT_NONE` with `b_TcpConnected == true` and a continuously increasing `ui32_CurrentTcpSessionDurationMs` is the desired steady state.
## TouchGFX target-build integration (alpha.6.1)

`TouchGFX/config/gcc/app.mk` is the authoritative, user-owned bridge between the generated TouchGFX GCC target and the application modules. It appends the complete `Application/` component set, including `Application/Interfaces/Inc`, so `Model.hpp` can resolve `hmi_service.h` during **Program and Run Target**. The generated `gcc/Makefile` must not be hand-edited; TouchGFX can regenerate it.

If a target build reports `hmi_service.h: No such file or directory`, confirm the following line family remains present in `TouchGFX/config/gcc/app.mk` and regenerate: `cubemx_components += Application/Common ... Application/Services/Modbus`.


### Alpha.6.2 TCP session-state policy

Socket 0 is now treated as authoritative. A single transient `Sn_SR` sample, PHYCFGR read anomaly, or bounded Modbus transaction failure cannot actively tear down a positively ESTABLISHED TCP session. Reconnect is initiated only after socket state is consistently non-established (or `CLOSE_WAIT` positively confirms a remote FIN). Use the new diagnostic disagreement/suppression counters when commissioning W5500 hardware.
## Alpha.7.3 TouchGFX build fix

If Designer output stops after `make -f simulator/gcc/Makefile assets -j8` and shows a Windows banner followed by a `...\TouchGFX>` prompt, an interactive `cmd.exe` was launched by the build wrapper. Alpha.7.3 removes that path completely. Run `Tools\PREPARE_TOUCHGFX_ENVIRONMENT.bat` once before Designer; Generate Assets itself uses only GNU Make file-existence checks and never launches `cmd.exe` or PowerShell.

