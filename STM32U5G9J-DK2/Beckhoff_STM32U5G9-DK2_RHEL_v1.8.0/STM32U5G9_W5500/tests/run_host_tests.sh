#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD="$ROOT/tests/.host_build"
rm -rf "$BUILD"
mkdir -p "$BUILD"

CC=${CC:-cc}
CXX=${CXX:-c++}
CFLAGS="-std=c11 -Wall -Wextra -Werror -pedantic"
CXXFLAGS="-std=c++14 -Wall -Wextra -Werror -pedantic"

CFG_INC="-I$ROOT/Application/Common/Inc -I$ROOT/Application/Config/Inc"
IO_INC="$CFG_INC -I$ROOT/Application/Services/IO/Inc"
DIAG_INC="$IO_INC -I$ROOT/Application/Services/Diagnostics/Inc"
W5500_INC="-I$ROOT/Application/Drivers/W5500/Inc -I$ROOT/Application/Drivers/Platform/Inc"
MODBUS_INC="$W5500_INC -I$ROOT/Application/Services/Modbus/Inc -I$ROOT/Application/Config/Inc"
HMI_INC="$DIAG_INC -I$ROOT/Application/Interfaces/Inc"

$CC $CFLAGS $IO_INC \
  "$ROOT/tests/test_scaling.c" \
  "$ROOT/Application/Config/Src/io_config.c" \
  "$ROOT/Application/Config/Src/io_config_validate.c" \
  "$ROOT/Application/Services/IO/Src/io_service.c" \
  -lm -o "$BUILD/test_scaling"
"$BUILD/test_scaling"

$CC $CFLAGS $IO_INC \
  "$ROOT/tests/test_io_config.c" \
  "$ROOT/Application/Config/Src/io_config.c" \
  "$ROOT/Application/Config/Src/io_config_validate.c" \
  "$ROOT/Application/Services/IO/Src/io_service.c" \
  -lm -o "$BUILD/test_io_config"
"$BUILD/test_io_config"

$CC $CFLAGS $MODBUS_INC \
  "$ROOT/tests/test_modbus_tcp.c" \
  "$ROOT/Application/Services/Modbus/Src/modbus_tcp_client.c" \
  -o "$BUILD/test_modbus_tcp"
"$BUILD/test_modbus_tcp"

$CC $CFLAGS $W5500_INC \
  "$ROOT/tests/test_w5500_driver.c" \
  "$ROOT/Application/Drivers/W5500/Src/w5500_driver.c" \
  -o "$BUILD/test_w5500_driver"
"$BUILD/test_w5500_driver"

$CXX $CXXFLAGS \
  -I"$ROOT/tests/gui_stubs" -I"$ROOT/TouchGFX/gui/include" $HMI_INC \
  "$ROOT/tests/test_touchgfx_binding.cpp" \
  "$ROOT/TouchGFX/gui/src/model/Model.cpp" \
  "$ROOT/TouchGFX/gui/src/screen1_screen/Screen1Presenter.cpp" \
  "$ROOT/TouchGFX/gui/src/screen1_screen/Screen1View.cpp" \
  -o "$BUILD/test_touchgfx_binding"
"$BUILD/test_touchgfx_binding"

$CC $CFLAGS $HMI_INC -c "$ROOT/Application/Config/Src/io_config.c" -o "$BUILD/sim_io_config.o"
$CC $CFLAGS $HMI_INC -c "$ROOT/Application/Config/Src/io_config_validate.c" -o "$BUILD/sim_io_config_validate.o"
$CC $CFLAGS $HMI_INC -c "$ROOT/Application/Services/IO/Src/io_service.c" -o "$BUILD/sim_io_service.o"
$CC $CFLAGS $HMI_INC -c "$ROOT/Application/Services/Diagnostics/Src/diagnostics_service.c" -o "$BUILD/sim_diagnostics_service.o"
$CXX $CXXFLAGS $HMI_INC \
  "$ROOT/tests/test_simulator_backend.cpp" \
  "$ROOT/TouchGFX/simulator/master_hmi_sim.cpp" \
  "$BUILD/sim_io_config.o" \
  "$BUILD/sim_io_config_validate.o" \
  "$BUILD/sim_io_service.o" \
  "$BUILD/sim_diagnostics_service.o" \
  -o "$BUILD/test_simulator_backend"
"$BUILD/test_simulator_backend"

echo 'HOST TESTS: PASS'
