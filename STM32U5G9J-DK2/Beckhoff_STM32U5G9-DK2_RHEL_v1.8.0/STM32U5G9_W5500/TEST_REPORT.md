# STM32 host validation

The packaged host suites pass for scaling, one-unit I/O configuration, Modbus TCP, W5500 mocks, TouchGFX MVC bindings and the simulator backend. The project and TouchGFX binding verifiers also pass.

Reproduce on Linux with:

```bash
bash tests/run_host_tests.sh
python3 Tools/verify_project.py
python3 Tools/verify_touchgfx_bindings.py
```

Native Windows TouchGFX generation, ARM compilation, flashing and hardware-in-the-loop tests remain required.
