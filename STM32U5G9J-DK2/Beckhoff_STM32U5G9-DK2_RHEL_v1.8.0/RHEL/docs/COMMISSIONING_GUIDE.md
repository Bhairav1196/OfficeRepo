# Commissioning guide

1. Assemble `EK9500 -> EL1409 -> EL2409 -> EL3074 -> EL4074` and set the EK9500 to `192.168.1.1`.
2. Verify RHEL can reach TCP 44818 and UDP 2222 on `192.168.1.1`. Allow inbound Modbus TCP only from approved masters.
3. Run `./scripts/build.sh`, add the STM32 master with `./scripts/masterctl.sh add-stm32 <STM32-IP>`, then run `./scripts/run.sh`.
4. Confirm the log reports Unit ID 1 and assemblies `30/22`. A different size means the live PDO layout does not match `config/coupler.conf`.
5. Verify all 16 DI and 16 DO, preserving unrelated DO bits during single-channel writes.
6. Inject known EL3074 values and verify FC04 addresses 1024..1027 and the TouchGFX AI view.
7. With an isolated current test load, write `132` to Holding Register address 0. Confirm `1.320 mA` in the HMI, `0.00132 A` in the EL4074 REAL32 PDO, and the DMM reading independently.
8. Verify AI/AO status at FC04 addresses 2048..2051 and 3072..3075.
9. Test cable removal, EK9500 power cycle, RHEL restart, stale-data rejection, and recovery without changing Unit ID.

Do not bypass strict assembly checking or modify scaling to compensate for wiring, terminal-mode, or calibration faults.
