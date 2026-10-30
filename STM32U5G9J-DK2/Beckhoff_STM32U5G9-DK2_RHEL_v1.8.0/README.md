# AO + / - and fine / coarse TouchGFX patch

For the STM32U5G9_W5500 single-coupler project. This archive contains the
matching Designer screen, labels, view header, and view implementation. Apply
all four files together; copying only Screen1View.cpp causes generation or
compile failures. No firmware, bus-coupler configuration, or other screens'
application logic is replaced.

1. Close TouchGFX Designer. Extract this ZIP to a temporary folder.
2. In PowerShell, from that extracted folder, run:

   ```powershell
   .\APPLY_AO_PATCH.ps1 -ProjectRoot 'C:\Users\Pratham\Downloads\Beckhoff_STM32_RHEL_v1.8.0_9\Beckhoff_STM32_RHEL_v1.8.0_SINGLE_COUPLER\STM32U5G9_W5500'
   ```

   If PowerShell blocks scripts, run it in the current process with
   `pwsh -ExecutionPolicy Bypass -File .\APPLY_AO_PATCH.ps1 -ProjectRoot '...\STM32U5G9_W5500'`.
   The installer checks the target project and backs up the four originals
   inside the target project before copying. Do not restore the older
   compile-only Screen1View.cpp afterward.

3. Open `TouchGFX/STM32U5G9_DK2_W5500_TouchGFX_ModbusTCP_Master.touchgfx`
   **from the patched project**, then select **Run Simulator**. Designer
   regenerates its own `generated/` code. Do not copy old generated files.
4. On the I/O page, the four AO cards each have `-` and `+` buttons. Tap the
   `COARSE 0.10 mA` button to switch to `FINE 0.01 mA`; tap again to return.
   Controls appear only while the selected AO is reported valid, enabled,
   and writable. The project simulator must be connected to its simulated
   I/O data for AO commands to be accepted. Values are bounded by the
   project's -20.00 to +20.00 mA operating range.

Verification (run in the project's `TouchGFX` directory):

```powershell
Select-String .\gui\src\screen1_screen\Screen1View.cpp -Pattern 'toggleAoStep|aoFineStep \? 1 : 10|showControl = false'
```

Expected: the first two patterns appear; `showControl = false` does not.
If the same old errors reappear, check that Designer has opened the patched
`.touchgfx` project and that the installer reported the correct project path.

The project's Linux host release suite passed, including strict C++
TouchGFX binding compilation. The actual Windows TouchGFX Designer / simulator
cannot be run in this workspace, so rerun Generate Code and Run Simulator
locally to verify the graphics and commands.
