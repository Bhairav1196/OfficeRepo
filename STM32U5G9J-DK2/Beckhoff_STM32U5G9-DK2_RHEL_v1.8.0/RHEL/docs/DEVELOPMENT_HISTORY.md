# Consolidated Development History and Learnings

## v1.8.0 — single-station consolidation

The two-route model was replaced by one EK9500 terminal train containing EL1409, EL2409, EL3074 and EL4074. One route worker and one Modbus Unit ID now own the complete digital and analog process image. The strict 30-byte input and 22-byte output sizes make terminal-order drift fail visibly during commissioning.

## Why v1.3.0 exists

The v1.2.x line solved several independent commissioning problems in sequence.
Keeping each fix as a patch made the final behaviour harder to understand from
one document. v1.3.0 makes the v1.2.4 code contract authoritative and records
why each retained design decision exists.

## v1.2.0 — multi-master dual-coupler baseline

The key architectural decision was to terminate Modbus TCP on RHEL and keep one
EtherNet/IP owner per EK9500. A non-blocking server allowed multiple masters to
poll independently. Runtime source-IP ACL changes could add/remove masters
without restarting EtherNet/IP routes. Tests established slow/fragmented-client
isolation and concurrent polling.

**Learning:** master concurrency belongs at the Modbus edge; EtherNet/IP device
ownership must remain singular and stable.

## v1.2.1 — operational configuration is mutable

The initial configuration test assumed a fixed ACL content. That conflicted with
the product requirement that operators can add/remove masters. The test was
changed to validate syntax/path rather than a particular master count.

**Learning:** regression tests must validate invariants, not mutable operations
data.

## v1.2.2 — dual listener, one data model

Beijer commonly expects standard Modbus TCP port 502 while existing ModScan work
used 1502. The correct solution was a second listener feeding the same server
state, not a second gateway process.

**Learning:** multiple TCP endpoints must not duplicate process images or
EtherNet/IP ownership. Privileged port 502 should use only
`CAP_NET_BIND_SERVICE`, not root execution.

## v1.2.3 — centralize AO conversion

AO writes from FC06, FC16 and FC23 needed one conversion function and symmetric
readback. This removed function-specific scaling divergence.

**Learning:** engineering-unit conversion must have one canonical implementation
used by every protocol write path.

## v1.2.4 — current units and SI REAL32

Commissioning showed a critical unit mismatch: a GUI command representing
1.32 mA became `132` as signed int16, but the Beckhoff REAL32 path was treated
as though 1.32 were already SI. The corrected contract is:

```text
132 counts * 0.01 mA/count = 1.32 mA = 0.00132 A
```

The EL4074 route was therefore configured as bipolar ±20 mA with SI engineering
range `-0.020..+0.020`, and the conversion divisor became 100000.

**Learning:** always name and test the unit at every boundary. “Engineering
value” is insufficient when one interface uses mA and another uses A.

## GUI iterations

Commissioning GUI iterations exposed two additional lessons:

- A display must not infer voltage/current meaning from a column position; it
  must follow the actual terminal/profile engineering unit.
- The Modbus AO map remains four signed command registers even though the
  EtherNet/IP PDO contains four REAL32 values. Expanding Modbus polling to eight
  registers causes Illegal Data Address and misrepresents the gateway contract.

The v1.3.0 read-only monitor implements these lessons directly.

## v1.3.0 — maintenance baseline

v1.3.0 does not intentionally redesign working protocol behaviour. It converts
accumulated commissioning knowledge into explicit invariants, tests, scripts and
field documentation so future changes can be evaluated against a single
well-defined baseline.

## v1.4.0 — STM32U5G9/W5100 master integration baseline

v1.4.0 preserves the v1.3.0 Modbus/EtherNet-IP data contract and adds the RHEL
support needed for the planned STM32U5G9J-DK2 + W5100 Modbus TCP master.

Key lessons incorporated:

- an embedded 50 Hz master can legitimately exceed the old global 200 req/s
  ceiling when several register blocks are polled each cycle;
- globally increasing that ceiling would unnecessarily relax ModScan/Beijer
  clients;
- the ACL therefore accepts an optional fourth per-master rate field;
- existing three-field ACL files remain valid;
- the STM32 profile is one persistent connection with 500 req/s protection
  headroom and an initial 20 ms polling period;
- the W5100 shield's microSD device is outside the RHEL boundary and is not used;
- the STM32 talks only to the RHEL Modbus server, not directly to the EK9500;
- the EL4074 Modbus wire contract remains signed int16 at 0.01 mA/count.

The host-side `stm32_w5100_contract_test.py` reproduces the intended persistent
five-read polling cycle and can optionally perform a controlled EL4074 write.

## v1.5.0 — STM32F407VGT6/LAN8720A master implementation baseline

v1.5.0 replaces the planned W5100 path with the hardware-validated
STM32F407VGT6 + LAN8720A RMII design and matches it to a complete ThreadX/NetX
Duo Modbus TCP master project.

Key lessons incorporated:

- native STM32F4 Ethernet MAC + LAN8720A is a better fit than forcing an SPI
  Ethernet controller into this design;
- LAN8720A PHY address was verified as 1 and PHY identity as `0x0007/0xC0F1`;
- BSR link status must be read twice because it is latch-low;
- the embedded master uses one persistent connection to RHEL TCP/502 and the
  same five-read 20 ms contract already modeled by the host tool;
- the STM32 source IPv4 baseline is `192.168.1.19`; RHEL test-interface baseline
  is `192.168.1.7`;
- no Modbus register-map or EL4074 scaling change was required;
- the current ACL helper creates `STM32F407_LAN8720|<IP>|1|500`; the old W5100
  helper is retained only for historical bench compatibility.

**Learning:** separate the physical Ethernet implementation from the Modbus
wire contract. Changing W5100/SPI to LAN8720/RMII required embedded driver work,
but did not require destabilizing the proven RHEL Modbus/EtherNet-IP data model.
