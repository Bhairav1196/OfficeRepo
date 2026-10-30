# Changelog

## 1.8.0 — 2026-09-11

- Replaced the two-coupler topology with one EK9500 at `192.168.1.1`, Modbus Unit ID 1.
- Combined EL1409 DI, EL2409 DO, EL3074 AI and EL4074 AO into one strict PDO definition.
- Set Assembly 129 to 30 bytes and Assembly 130 to 22 bytes.
- Removed the second route, second configuration file and duplicate-route assumptions.
- Updated the Modbus CLI, live monitor, simulator and integration tests for the single unit.
- Retained AO scaling: signed register counts at 0.01 mA/count, encoded to EL4074 REAL32 amperes.

## 1.7.0 — 2026-09-11

- Native gateway baseline, access control, coherent route transactions, cyclic health checks and integration coverage.
