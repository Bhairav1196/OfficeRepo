# Troubleshooting

| Symptom | Check |
|---|---|
| Route never opens | EK9500 is `192.168.1.1`; TCP 44818 and UDP 2222 are permitted; terminal order matches the release. |
| Assembly-size rejection | Confirm Assembly 129 = 30 bytes and Assembly 130 = 22 bytes. Reconcile the actual PDO; keep strict checking enabled. |
| Modbus exception 0x0B or 0x06 | Inspect route freshness, ACL/rate-limit messages and cyclic UDP traffic. |
| DI/DO shifted | EL1409 must be position 1 and EL2409 position 2. |
| AI invalid | EL3074 must be position 3 and use the configured bipolar ±10 V mode. Check status registers 2048–2051. |
| AO invalid | EL4074 must be position 4 and use bipolar ±20 mA. Check status registers 3072–3075. |
| STM32 cannot connect | Confirm RHEL `192.168.1.7:502`, STM32 `192.168.1.60/24`, cable/VLAN, W5500 link and ACL entry. |

Use `./build/ek9500_modbus_tcp_gateway --check-config config/gateway.conf` before each deployment. Do not energize field outputs until channel mapping, safe states and scaling have been witnessed.
