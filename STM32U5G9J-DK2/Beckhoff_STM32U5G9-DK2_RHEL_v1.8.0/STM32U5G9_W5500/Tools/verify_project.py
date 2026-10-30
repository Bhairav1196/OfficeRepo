#!/usr/bin/env python3
"""Static release-contract verifier for the v2 modular project.

@author Prathamesh Chipkar
@version 2.0.0-alpha.9.0
"""
from pathlib import Path
import json
import re
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]).resolve()
checks = []

def ck(description, condition):
    checks.append((description, bool(condition)))


def read(rel):
    p = root / rel
    return p.read_text(encoding='utf-8', errors='ignore') if p.exists() else ''

version = read('VERSION').strip()
app_version = read('Application/Common/Inc/app_version.h')
app_threadx = read('Core/Src/app_threadx.c')
main = read('Core/Src/main.c')
ioc = read('STM32U5G9J-DK2-ThreadX.ioc')
extsettings = read('.extSettings')
project = read('STM32CubeIDE/.project')
cproject = read('STM32CubeIDE/.cproject')
view = read('TouchGFX/gui/src/screen1_screen/Screen1View.cpp')
view_h = read('TouchGFX/gui/include/gui/screen1_screen/Screen1View.hpp')
sim_mk = read('TouchGFX/simulator/gcc/Makefile')
target_app_mk = read('TouchGFX/config/gcc/app.mk')
sim_backend = read('TouchGFX/simulator/master_hmi_sim.cpp')

ck('SemVer VERSION is 2.0.0-alpha.9.0', version == '2.0.0-alpha.9.0')
ck('app_version.h matches VERSION', f'APP_VERSION_STRING       "{version}"' in app_version)
prep_ps = read('Tools/prepare_touchgfx_tools.ps1')
sim_mk = read('TouchGFX/simulator/gcc/Makefile')
ck('TouchGFX tool prep has no recursive scan', '-Recurse' not in prep_ps or 'Copy-Item' in prep_ps and 'Get-ChildItem' not in prep_ps)
ck('TouchGFX asset build uses non-interactive GNU Make checks', '$(wildcard ../Middlewares/ST/touchgfx/framework/tools/imageconvert/build/win/imageconvert.out)' in sim_mk and '$(error ERROR: TouchGFX imageconvert.out is missing.' in sim_mk)
ck('TouchGFX asset build does not invoke PREPARE script', 'assets: prepare_touchgfx_environment' not in sim_mk)

ck('ThreadX hook includes app_rtos.h once', app_threadx.count('#include "app_rtos.h"') == 1)
ck('ThreadX hook calls fn_AppRtos_Create once', app_threadx.count('fn_AppRtos_Create()') == 1)
ck('legacy Master_ThreadX_Create removed', 'Master_ThreadX_Create' not in app_threadx)
ck('SPI1 generated init is 8-bit', 'SPI_DATASIZE_8BIT' in main)
ck('SPI1 generated init prescaler is /8', 'SPI_BAUDRATEPRESCALER_8' in main)
ck('IOC SPI1 remains 8-bit', 'SPI1.DataSize=SPI_DATASIZE_8BIT' in ioc)
ck('IOC W5500 CS normalized', 'PB0.GPIO_Label=W5500_CS' in ioc)
ck('IOC W5500 reset normalized', ('PB1.GPIO_Label=W5500_RST' in ioc) or ('W5500_RST' in ioc))

required_app_files = [
    'Application/Common/Inc/app_version.h',
    'Application/Config/Inc/app_config.h',
    'Application/Config/Inc/io_config.h',
    'Application/Config/Inc/io_config_types.h',
    'Application/Config/Inc/io_signal_ids.h',
    'Application/Config/Inc/network_config.h',
    'Application/Config/Inc/modbus_config.h',
    'Application/Config/Src/io_config.c',
    'Application/Config/Src/io_config_validate.c',
    'Application/Drivers/Platform/Inc/w5500_port_stm32.h',
    'Application/Drivers/Platform/Src/w5500_port_stm32.c',
    'Application/Drivers/W5500/Inc/w5500_driver.h',
    'Application/Drivers/W5500/Src/w5500_driver.c',
    'Application/Interfaces/Inc/hmi_service.h',
    'Application/RTOS/Inc/app_rtos.h',
    'Application/RTOS/Inc/app_watchdog.h',
    'Application/RTOS/Src/app_rtos.c',
    'Application/RTOS/Src/app_watchdog.c',
    'Application/Services/Diagnostics/Inc/diagnostics_service.h',
    'Application/Services/Diagnostics/Src/diagnostics_service.c',
    'Application/Services/IO/Inc/io_service.h',
    'Application/Services/IO/Src/io_service.c',
    'Application/Services/Modbus/Inc/modbus_tcp_client.h',
    'Application/Services/Modbus/Src/modbus_tcp_client.c',
]
for rel in required_app_files:
    ck(rel + ' present', (root / rel).exists())

legacy_core = ['master_io.c','master_threadx.c','modbus_tcp_client.c','w5500_master.c','w5500_port_stm32.c']
for name in legacy_core:
    ck('legacy Core/Src/' + name + ' absent', not (root / 'Core/Src' / name).exists())

for p in sorted((root / 'Application').rglob('*')):
    if p.is_file() and p.suffix.lower() in {'.c','.h','.cpp','.hpp'}:
        text = p.read_text(encoding='utf-8', errors='ignore')
        ck(str(p.relative_to(root)) + ' has author', '@author  Prathamesh Chipkar' in text or '@author Prathamesh Chipkar' in text)
        ck(str(p.relative_to(root)) + ' has version', '@version 2.0.0-alpha.9.0' in text or '@version     2.0.0-alpha.9.0' in text)

# Configuration model checks.
io_types = read('Application/Config/Inc/io_config_types.h')
io_cfg = read('Application/Config/Src/io_config.c')
io_validator = read('Application/Config/Src/io_config_validate.c')
io_service = read('Application/Services/IO/Src/io_service.c')
ck('current and voltage modes configured', all(x in io_types for x in [
    'E_IO_ANALOG_CURRENT_BIPOLAR_20MA','E_IO_ANALOG_CURRENT_0_20MA','E_IO_ANALOG_CURRENT_4_20MA',
    'E_IO_ANALOG_VOLTAGE_BIPOLAR_10V','E_IO_ANALOG_VOLTAGE_0_10V']))
ck('piecewise mapping supported', 'E_IO_MAPPING_PIECEWISE_LINEAR' in io_types and 'past_Calibration' in io_types)
ck('write reject/clamp policy supported', 'E_IO_WRITE_REJECT' in io_types and 'E_IO_WRITE_CLAMP' in io_types)
ck('configuration has electrical hardware/operating/map ranges', all(x in io_types for x in [
    'st_ElectricalHardwareRange','st_ElectricalOperatingRange','st_ElectricalMapRange']))
ck('configuration has process map/operating ranges', 'st_ProcessMapRange' in io_types and 'st_ProcessOperatingRange' in io_types)
ck('configuration fixed to one Beckhoff unit', 'APP_CFG_BECKHOFF_UNIT_COUNT' in read('Application/Config/Inc/app_config.h') and '(1U)' in read('Application/Config/Inc/app_config.h'))
ck('digital HMI exposes 16 slots', 'APP_CFG_DIGITAL_HMI_SLOTS' in read('Application/Config/Inc/app_config.h') and '(16U)' in read('Application/Config/Inc/app_config.h'))
ck('Unit 1 topology is 16 DI 16 DO 4 AI 4 AO', '16 DI  |  16 DO  |  4 AI  |  4 AO' in io_cfg)
ck('Unit 1 AI default mode is +/-10 V', io_cfg.count('E_IO_ANALOG_VOLTAGE_BIPOLAR_10V') >= 4)
ck('Unit 1 AO default mode is +/-20 mA', io_cfg.count('E_IO_ANALOG_CURRENT_BIPOLAR_20MA') >= 4)
ck('duplicate register mapping validation present', 'E_IO_CONFIG_DUPLICATE_MAPPING' in io_validator)
ck('AO service rejects out-of-range before register write', 'E_IO_STATUS_OUT_OF_RANGE' in io_service and 'st_ProcessOperatingRange' in io_service)
ck('TouchGFX metadata comes from unit-aware config', 'fn_IOConfig_FindUnitHmiSlot' in sim_backend and 'pc_ProcessUnit' in sim_backend)

# TouchGFX descriptor: generated code may be stale by design until Designer Generate Code.
touchgfx_files = list((root / 'TouchGFX').glob('*.touchgfx'))
ck('exactly one TouchGFX descriptor', len(touchgfx_files) == 1)
if touchgfx_files:
    data = json.loads(touchgfx_files[0].read_text(encoding='utf-8'))
    screens = data.get('Application',{}).get('Screens',[])
    ck('TouchGFX 800x480', data.get('Application',{}).get('Resolution') == {'Width':800,'Height':480})
    ck('TouchGFX one state-driven six-page root screen', len(screens) == 1 and screens[0].get('Name') == 'Screen1' and 'showPage(uint8_t page)' in view_h)
    components = screens[0].get('Components',[]) if screens else []
    names = {c.get('Name') for c in components}
    ck('TouchGFX maritime single-unit HMI has 231 Designer-owned components', len(components) == 231)
    types = {}
    for c in components:
        types[c.get('Type')] = types.get(c.get('Type'), 0) + 1
    ck('TouchGFX has 10 rounded Button components', types.get('Button', 0) == 10)
    expected = {'tcpValue','uptimeValue','unit1Button','unit2Button','unitDescription',
                'diName01','diName16','diValue16','doName01','doName16','doValue16',
                'analogName01','analogName04','analogValue01','analogValue04',
                'aoMinus01','aoPlus01','aoMinus04','aoPlus04'}
    ck('TouchGFX single-unit process widgets present', expected.issubset(names))
    interactions = screens[0].get('Interactions', [])
    ck('TouchGFX unit/AO interactions count is 10', len(interactions) == 10)
    image_refs = ' '.join(str(c) for c in components)
    ck('TouchGFX rounded metallic tab assets referenced', 'metallic_tab_released.png' in image_refs and 'metallic_tab_pressed.png' in image_refs)
    ck('TouchGFX rounded metallic AO assets referenced', 'metallic_small_released.png' in image_refs and 'metallic_small_pressed.png' in image_refs)

ck('TouchGFX uses supported Color::getColorFromRGB', 'getColorFrom24BitRGB' not in view and 'getColorFromRGB' in view)
ck('TouchGFX owns live buffers outside generated code', 'tcpValueBuffer[24]' in view_h and 'diNameBuffer[HMI_DIGITAL_SLOT_COUNT][32]' in view_h and 'analogValueBuffer[HMI_ANALOG_SLOT_COUNT][32]' in view_h)
ck('TouchGFX explicit wildcard binding present', 'bindLiveTextBuffers()' in view and '.setWildcard(' in view)
ck('TouchGFX main screen uses process model not raw fields', 'i32_RawValue' not in view and 'f32_ElectricalValue' not in view)
ck('TouchGFX analog formatter avoids unsupported %lu length modifier', '%lu' not in view)
ck('TouchGFX shows AI and AO simultaneously', 'frame.aiValid[slot]' in view and 'frame.aoValid[slot]' in view)
ck('TouchGFX has all six maritime pages', all(x in view for x in ['MOTOR STARTER PANEL','ICGS VESSEL SENSORS HUB','FIN STABILIZER LOCAL CONTROL','MACHINERY CONTROL ROOM','BRIDGE CONSOLE']))
ck('TouchGFX dark tactical palette', 'getColorFromRGB(11, 15, 25)' in view and 'getColorFromRGB(0, 229, 255)' in view)
ck('RHEL matched rate ceiling is 500 req/s', 'NET_CFG_RHEL_EXPECTED_RATE_LIMIT (500U)' in read('Application/Config/Inc/network_config.h'))
rtos = read('Application/RTOS/Src/app_rtos.c')
w5500 = read('Application/Drivers/W5500/Src/w5500_driver.c')
ck('server refusal does not trigger W5500 reinit', 'ui32_ConnectFailures >= APP_CFG_CONNECT_FAILURE_REINIT' not in rtos)
ck('valid Modbus exceptions have dedicated counter', 'ui32_ModbusExceptions' in rtos and '++pst_Current->ui32_ProtocolErrors' not in rtos.split('if (e_Result == E_MBTCP_EXCEPTION)',1)[1].split('else if',1)[0])
ck('W5500 connect guards established session', 'established_seen' in w5500 and 'fn_W5500_AbortSocket' in w5500)
ck('IWDG starts before ThreadX', 'fn_AppWatchdog_Init();' in main and main.index('fn_AppWatchdog_Init();') < main.index('MX_ThreadX_Init();'))
ck('IWDG refresh is task-health gated', 's_ui32_TransportHeartbeat != ui32_LastTransportCycle' in rtos and 's_ui32_DiagnosticsHeartbeat != ui32_LastDiagnosticsCycle' in rtos and 'fn_AppWatchdog_Refresh();' in rtos)
ck('IWDG configured in CubeMX descriptor', 'Mcu.IP21=IWDG' in ioc and 'IWDG.Prescaler=IWDG_PRESCALER_64' in ioc)

ck('Simulator links v2 config source', '../Application/Config/Src/io_config.c' in sim_mk)
ck('Simulator links v2 IO service', '../Application/Services/IO/Src/io_service.c' in sim_mk)
ck('Simulator includes HMI interface path', '../Application/Interfaces/Inc' in sim_mk)
ck('Target app.mk integrates HMI interface', 'Application/Interfaces' in target_app_mk)
ck('Target app.mk integrates RTOS implementation', 'Application/RTOS' in target_app_mk)
ck('Target app.mk integrates Modbus service', 'Application/Services/Modbus' in target_app_mk)
ck('Target app.mk integrates W5500 driver', 'Application/Drivers/W5500' in target_app_mk)
ck('Simulator compiles application C sources with gcc', 'c_compiler=gcc' in sim_mk)
ck('Simulator defines C-only option override', 'TOUCHGFX_C_OPTIONS' in sim_mk and 'c_compiler_options_local="$(TOUCHGFX_C_OPTIONS)"' in sim_mk)
# The C-only option block must not contain C++-only warnings. Generated TouchGFX C++
# flags remain untouched and can still use overloaded-virtual diagnostics.
_c_option_block = sim_mk.split('TOUCHGFX_C_OPTIONS :=', 1)[1].split('all clean assets:', 1)[0] if 'TOUCHGFX_C_OPTIONS :=' in sim_mk else ''
ck('Simulator C-only option list excludes overloaded-virtual', 'overloaded-virtual' not in _c_option_block)
tool_ps1 = read('Tools/prepare_touchgfx_tools.ps1')
tool_bat = read('Tools/PREPARE_TOUCHGFX_TOOLS.bat')
env_bat = read('Tools/PREPARE_TOUCHGFX_ENVIRONMENT.bat')
ck('TouchGFX asset check spawns no interactive Windows shell', '\t@cmd.exe' not in sim_mk and '\tcmd.exe' not in sim_mk and '\tpowershell.exe' not in sim_mk)
ck('TouchGFX build path does not execute environment preparation helper', '@cmd.exe /D /C "..\\Tools\\PREPARE_TOUCHGFX_ENVIRONMENT.bat"' not in sim_mk)
ck('TouchGFX tool preparation batch exists', bool(tool_bat))
ck('TouchGFX environment preparation batch exists', 'PREPARE_TOUCHGFX_TOOLS.bat' in env_bat and 'PREPARE_TOUCHGFX_FONT.bat' in env_bat)
ck('TouchGFX tool locator requires 4.26.1', '$RequiredTouchGfxVersion = 42601' in tool_ps1)
ck('TouchGFX tool locator validates Version.hpp', 'Get-TouchGfxVersion' in tool_ps1 and 'TOUCHGFX_VERSION' in tool_ps1)
ck('TouchGFX tool locator handles imageconvert and fontconvert', 'imageconvert.out' in tool_ps1 and 'fontconvert.out' in tool_ps1)
ck('TouchGFX tool locator supports official local override', 'TOUCHGFX_TOOL_SOURCE' in tool_ps1)
ck('TouchGFX vendor converter binaries are not redistributed', not any(x.is_file() for x in (root/'Middlewares/ST/touchgfx/framework/tools/imageconvert').rglob('*.out')) and not any(x.is_file() for x in (root/'Middlewares/ST/touchgfx/framework/tools/fontconvert').rglob('*.out')))
ck('Simulator backend at non-duplicated root path', (root/'TouchGFX/simulator/master_hmi_sim.cpp').exists() and not (root/'TouchGFX/simulator/src/master_hmi_sim.cpp').exists())
ck('Simulator backend has no HAL/ThreadX dependency', 'stm32u5' not in sim_backend.lower() and 'tx_api.h' not in sim_backend and 'HAL_' not in sim_backend)

# CubeIDE metadata must show headers and compile all application-owned C sources.
for rel in required_app_files:
    if rel.endswith(('.c','.h')):
        ck('CubeIDE linked: ' + rel, f'PARENT-1-PROJECT_LOC/{rel}' in project)
include_dirs = [
    '../../Application/Common/Inc','../../Application/Config/Inc','../../Application/Drivers/Platform/Inc',
    '../../Application/Drivers/W5500/Inc','../../Application/Interfaces/Inc','../../Application/RTOS/Inc',
    '../../Application/Services/Diagnostics/Inc','../../Application/Services/IO/Inc','../../Application/Services/Modbus/Inc']
for inc in include_dirs:
    ck('CubeIDE include path: ' + inc, inc in cproject)
    ck('extSettings header path: ' + inc.replace('../../','../'), inc.replace('../../','../') in extsettings)
for legacy in legacy_core:
    ck('CubeIDE stale link removed: ' + legacy, f'Core/Src/{legacy}' not in project)

texts_xml = read('TouchGFX/assets/texts/texts.xml')
font_helper = read('Tools/PREPARE_TOUCHGFX_FONT.bat')
font_helper_ps1 = read('Tools/prepare_touchgfx_font.ps1')
ck('TouchGFX primary font is Roboto', 'Font="Roboto-Regular.ttf"' in texts_xml and 'Font="Roboto-Medium.ttf"' in texts_xml)
ck('TouchGFX no Verdana typography remains', 'verdana' not in texts_xml.lower())
ck('font helper launches PowerShell preparation', 'prepare_touchgfx_font.ps1' in font_helper.lower())
ck('font helper prepares Roboto', 'roboto-regular.ttf' in font_helper_ps1.lower() and 'roboto-medium.ttf' in font_helper_ps1.lower())
ck('font helper supports official download', 'https://fonts.google.com/download?family=roboto' in font_helper_ps1.lower())
ck('font helper validates font binaries', 'test-truetypefont' in font_helper_ps1.lower())

# Packaging safety: do not ship font binaries.
font_files = [p for p in root.rglob('*') if p.is_file() and p.suffix.lower() in {'.ttf','.otf','.woff','.woff2'}]
ck('no font binaries packaged', len(font_files) == 0)

failed = [x for x in checks if not x[1]]
for desc, ok in checks:
    print(('PASS' if ok else 'FAIL') + ': ' + desc)
if failed:
    print(f'PROJECT VERIFY: FAIL ({len(failed)} checks)', file=sys.stderr)
    raise SystemExit(1)
print(f'PROJECT VERIFY: PASS ({len(checks)} checks)')
