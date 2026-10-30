# RHEL gateway v1.8.0 release notes

This release supports one fixed Beckhoff station only:

`EK9500 -> EL1409 -> EL2409 -> EL3074 -> EL4074`

All DI, DO, AI and AO are exposed through Modbus Unit ID 1. The default EK9500 address is `192.168.1.1`; strict assembly checking expects 30 input bytes and 22 output bytes.

Host regression and executable loopback integration tests pass. Physical EK9500, terminal wiring, RHEL deployment and endurance acceptance remain commissioning tasks.
