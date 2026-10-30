# Verification Strategy — v1.5.0

## Standard verification

```bash
./scripts/build.sh --clean
```

This configures a strict ISO C11 build and executes the CTest suite.

## Extended verification

```bash
./scripts/verify.sh --full
```

Additional checks include shell syntax, Python monitor and STM32-contract-tool
compilation/self-tests, version reporting, GCC static analyzer, AddressSanitizer
and UndefinedBehaviorSanitizer.

## CTest responsibilities

- `configuration`: validates gateway/coupler/ACL configuration assumptions.
- `modbus_protocol`: validates Modbus functions, route/address legality and
  core data-model translations.
- `modbus_multi_client`: exercises concurrent Modbus master polling.
- `master_acl`: validates ACL parser cases.
- `master_access_control`: validates authorization, reload, removal and client
  isolation semantics.
- `dual_listener`: proves two listener sockets serve the same route/data model.
- `ao_scaling`: locks signed 0.01 mA/count to SI amperes conversion, including
  `132 -> 0.00132 A`, negative values, bounds and invalid input.
- `masterctl_stm32_profile`: validates creation/list/removal of the dedicated
  STM32F407/LAN8720 ACL profile while preserving the normal ACL file semantics.
- `master_rate_limit`: runs the production server and proves the optional ACL
  request-rate ceiling overrides a higher gateway-wide fallback for that source.

## Static/sanitizer scope

`--full` builds the complete targets with GCC `-fanalyzer`, then rebuilds with
ASan/UBSan and executes the same tests. These checks detect many memory,
undefined-behaviour and control-flow defects but cannot establish real-time or
network timing on physical hardware.

## Installation check

A release verification should also stage-install the project and confirm the
binary, configuration, service unit, monitor, STM32 contract tool and documentation appear in the
expected paths.

## Evidence boundary

Host tests do not prove:

- actual EK9500 process-image configuration;
- physical wiring or terminal ordering;
- analog accuracy/calibration;
- Beijer driver address-base settings;
- plant communication-loss safe state;
- EMI/EMC or environmental behaviour.

Those require target-system commissioning and, where applicable, formal V&V.
