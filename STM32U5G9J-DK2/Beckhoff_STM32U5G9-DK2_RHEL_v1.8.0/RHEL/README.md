# EK9500 native C gateway — v1.8.0

The RHEL process exposes one EK9500 containing DI, DO, AI and AO as Modbus TCP Unit ID 1. It opens one EtherNet/IP Class-1 connection and may listen on both TCP 1502 and TCP 502; the two TCP listeners share the same Unit ID and process image.

## Build and run

```bash
./scripts/build.sh
./scripts/run.sh
```

No earlier project or online repository is required. To use TCP 502 as a non-root service, run `sudo ./scripts/enable_port_502.sh` after building.

## Coupler configuration

The authoritative file is `config/coupler.conf`.

| Terminal | Position | Process data |
|---|---:|---|
| EL1409 | 1 | 16 DI; input bytes 4..5 |
| EL2409 | 2 | 16 DO; output bytes 4..5 |
| EL3074 | 3 | 4 AI; input bytes 6..21 |
| EL4074 | 4 | 4 AO status in input bytes 22..29; four REAL32 commands in output bytes 6..21 |

Assembly sizes are Input 30 bytes and Output 22 bytes, including the configured four-byte run/idle headers. Strict size validation remains enabled.

## Modbus map

See `docs/REGISTER_MAP.md`. All functions use Unit ID 1. Unit ID 2 is intentionally unconfigured and returns Gateway Target Failed.

## Verification

```bash
make clean
make -j4
make test
python3 tools/ek9500_modbus_live_monitor.py --self-test
```

The test suite covers configuration, protocol framing, AO scaling, ACLs, rate limits, multiple clients, both listeners, and a loopback EtherNet/IP integration model.

## Commissioning boundary

Host tests do not prove the live EK9500 PDO layout. Confirm the actual terminal chain, assembly sizes, channel modes, wiring, and measured analog values before operational use.
