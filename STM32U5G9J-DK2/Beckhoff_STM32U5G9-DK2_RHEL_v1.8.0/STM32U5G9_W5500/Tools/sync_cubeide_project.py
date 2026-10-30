#!/usr/bin/env python3
"""
Synchronize the application-owned v2 source/header tree into STM32CubeIDE metadata.

This tool deliberately edits only STM32CubeIDE/.project and .cproject. It does
not modify CubeMX- or TouchGFX-generated C/C++ source files.

@author Prathamesh Chipkar
@version 2.0.0-alpha.9.0
"""
from __future__ import annotations

from pathlib import Path
import re
import sys

ROOT = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]).resolve()
PROJECT = ROOT / "STM32CubeIDE" / ".project"
CPROJECT = ROOT / "STM32CubeIDE" / ".cproject"

APP_INCLUDE_DIRS = [
    "../../Application/Common/Inc",
    "../../Application/Config/Inc",
    "../../Application/Drivers/Platform/Inc",
    "../../Application/Drivers/W5500/Inc",
    "../../Application/Interfaces/Inc",
    "../../Application/RTOS/Inc",
    "../../Application/Services/Diagnostics/Inc",
    "../../Application/Services/IO/Inc",
    "../../Application/Services/Modbus/Inc",
]

APP_FILES = [
    "Application/Common/Inc/app_version.h",
    "Application/Config/Inc/app_config.h",
    "Application/Config/Inc/io_config.h",
    "Application/Config/Inc/io_config_types.h",
    "Application/Config/Inc/io_signal_ids.h",
    "Application/Config/Inc/modbus_config.h",
    "Application/Config/Inc/network_config.h",
    "Application/Config/Src/io_config.c",
    "Application/Config/Src/io_config_validate.c",
    "Application/Drivers/Platform/Inc/w5500_port_stm32.h",
    "Application/Drivers/Platform/Src/w5500_port_stm32.c",
    "Application/Drivers/W5500/Inc/w5500_driver.h",
    "Application/Drivers/W5500/Src/w5500_driver.c",
    "Application/Interfaces/Inc/hmi_service.h",
    "Application/RTOS/Inc/app_rtos.h",
    "Application/RTOS/Inc/app_watchdog.h",
    "Application/RTOS/Src/app_rtos.c",
    "Application/RTOS/Src/app_watchdog.c",
    "Application/Services/Diagnostics/Inc/diagnostics_service.h",
    "Application/Services/Diagnostics/Src/diagnostics_service.c",
    "Application/Services/IO/Inc/io_service.h",
    "Application/Services/IO/Src/io_service.c",
    "Application/Services/Modbus/Inc/modbus_tcp_client.h",
    "Application/Services/Modbus/Src/modbus_tcp_client.c",
]

LEGACY_CORE_FILES = {
    "Core/Src/master_io.c",
    "Core/Src/master_threadx.c",
    "Core/Src/modbus_tcp_client.c",
    "Core/Src/w5500_master.c",
    "Core/Src/w5500_port_stm32.c",
}


def fail(message: str) -> None:
    raise SystemExit(f"ERROR: {message}")


def sync_project() -> None:
    if not PROJECT.exists():
        fail(f"missing {PROJECT}")
    text = PROJECT.read_text(encoding="utf-8")

    # Remove obsolete v1 linked-resource blocks that now point to deleted Core files.
    link_pattern = re.compile(r"\s*<link>.*?</link>", re.DOTALL)
    def keep_or_remove(match: re.Match[str]) -> str:
        block = match.group(0)
        return "" if any(f"PARENT-1-PROJECT_LOC/{p}" in block for p in LEGACY_CORE_FILES) else block
    text = link_pattern.sub(keep_or_remove, text)

    if "<linkedResources>" not in text:
        fail(".project has no <linkedResources> section")

    additions = []
    for rel in APP_FILES:
        abs_path = ROOT / rel
        if not abs_path.exists():
            fail(f"required application file missing: {rel}")
        uri = f"PARENT-1-PROJECT_LOC/{rel}"
        if uri in text:
            continue
        display = f"Application/v2/{rel[len('Application/'):]}"
        additions.append(
            "\n\t\t<link>\n"
            f"\t\t\t<name>{display}</name>\n"
            "\t\t\t<type>1</type>\n"
            f"\t\t\t<locationURI>{uri}</locationURI>\n"
            "\t\t</link>"
        )

    if additions:
        text = text.replace("\n\t</linkedResources>", "".join(additions) + "\n\t</linkedResources>", 1)
    PROJECT.write_text(text, encoding="utf-8")


def sync_cproject() -> None:
    if not CPROJECT.exists():
        fail(f"missing {CPROJECT}")
    text = CPROJECT.read_text(encoding="utf-8")

    # Add include paths to every C and C++ compiler include-path option in Debug/Release.
    option_pattern = re.compile(
        r'(<option\b[^>]*superClass="[^"]*(?:c\.compiler|cpp\.compiler)\.option\.includepaths[^"]*"[^>]*>)(.*?)(</option>)',
        re.DOTALL,
    )

    def add_includes(match: re.Match[str]) -> str:
        start, body, end = match.groups()
        for inc in APP_INCLUDE_DIRS:
            if f'value="{inc}"' not in body:
                body += f'\n\t\t\t\t\t\t\t\t\t<listOptionValue builtIn="false" value="{inc}"/>'
        return start + body + end

    new_text, count = option_pattern.subn(add_includes, text)
    if count == 0:
        fail("no C/C++ include-path options found in .cproject")
    CPROJECT.write_text(new_text, encoding="utf-8")


def main() -> None:
    sync_project()
    sync_cproject()
    print("CubeIDE project synchronization: PASS")


if __name__ == "__main__":
    main()
