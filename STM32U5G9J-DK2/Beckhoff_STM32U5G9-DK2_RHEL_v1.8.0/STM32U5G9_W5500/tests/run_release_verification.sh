#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/u5_w5500_v2_verify.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM

python3 "$ROOT/Tools/sync_cubeide_project.py" "$ROOT"
python3 "$ROOT/Tools/reapply_threadx_hook.py" "$ROOT"
python3 "$ROOT/Tools/verify_project.py" "$ROOT"
python3 "$ROOT/Tools/verify_touchgfx_bindings.py"
python3 "$ROOT/Tools/verify_touchgfx_c_compile.py" "$ROOT"
"$ROOT/tests/run_host_tests.sh"

CC=${CC:-cc}
CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic"
INCLUDES="-I$ROOT/tests/stubs -I$ROOT/Core/Inc \
-I$ROOT/Application/Common/Inc -I$ROOT/Application/Config/Inc \
-I$ROOT/Application/Drivers/Platform/Inc -I$ROOT/Application/Drivers/W5500/Inc \
-I$ROOT/Application/Interfaces/Inc -I$ROOT/Application/RTOS/Inc \
-I$ROOT/Application/Services/Diagnostics/Inc -I$ROOT/Application/Services/IO/Inc \
-I$ROOT/Application/Services/Modbus/Inc"

for src in \
  Application/Config/Src/io_config.c \
  Application/Config/Src/io_config_validate.c \
  Application/Drivers/Platform/Src/w5500_port_stm32.c \
  Application/Drivers/W5500/Src/w5500_driver.c \
  Application/RTOS/Src/app_rtos.c \
  Application/RTOS/Src/app_watchdog.c \
  Application/Services/Diagnostics/Src/diagnostics_service.c \
  Application/Services/IO/Src/io_service.c \
  Application/Services/Modbus/Src/modbus_tcp_client.c
do
  obj=$(printf '%s' "$src" | tr '/.' '__').o
  $CC $CFLAGS -DAPP_WATCHDOG_HOST_TEST $INCLUDES -c "$ROOT/$src" -o "$BUILD/$obj"
done

# Guard against the known TouchGFX regressions from v1.x.
if grep -q 'getColorFrom24BitRGB' "$ROOT/TouchGFX/gui/src/screen1_screen/Screen1View.cpp"; then
  echo 'FAIL: obsolete TouchGFX color API remains' >&2
  exit 1
fi
if ! grep -q 'bindLiveTextBuffers' "$ROOT/TouchGFX/gui/src/screen1_screen/Screen1View.cpp"; then
  echo 'FAIL: user-owned TouchGFX live-buffer binding missing' >&2
  exit 1
fi

# The generated GUI folder is owned by TouchGFX Designer and may be stale in a source package.
echo 'NOTE: Run TouchGFX Designer -> Generate Code before the first CubeIDE target build.'
echo 'RELEASE VERIFICATION: PASS'
