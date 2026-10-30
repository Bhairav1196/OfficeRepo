#!/usr/bin/env python3
"""STM32F407VGT6 + LAN8720A Modbus TCP contract/commissioning utility.

This host-side utility exercises the same persistent Modbus TCP transaction
pattern intended for the STM32F407VGT6 + LAN8720A master. It is deliberately
stdlib-only and read-only unless --write-ao is supplied.
"""
from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from dataclasses import dataclass

VERSION = "1.5.0"


@dataclass(frozen=True)
class MbResponse:
    transaction_id: int
    unit_id: int
    function_code: int
    pdu: bytes


def build_request(transaction_id: int, unit_id: int, function_code: int,
                  address: int, quantity_or_value: int) -> bytes:
    pdu = struct.pack(">BHH", function_code, address, quantity_or_value)
    mbap = struct.pack(">HHHB", transaction_id & 0xFFFF, 0, len(pdu) + 1, unit_id)
    return mbap + pdu


def recv_exact(sock: socket.socket, count: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < count:
        chunk = sock.recv(count - len(chunks))
        if not chunk:
            raise ConnectionError("peer closed before complete Modbus response")
        chunks.extend(chunk)
    return bytes(chunks)


def transact(sock: socket.socket, request: bytes) -> MbResponse:
    txid, protocol, _length, unit_id = struct.unpack(">HHHB", request[:7])
    if protocol != 0:
        raise ValueError("request protocol ID must be zero")
    sock.sendall(request)
    header = recv_exact(sock, 7)
    rx_txid, rx_protocol, rx_length, rx_unit = struct.unpack(">HHHB", header)
    if rx_protocol != 0 or rx_txid != txid or rx_unit != unit_id or rx_length < 2:
        raise RuntimeError("invalid MBAP response")
    pdu = recv_exact(sock, rx_length - 1)
    fc = pdu[0]
    if fc & 0x80:
        code = pdu[1] if len(pdu) > 1 else -1
        raise RuntimeError(f"Modbus exception FC=0x{fc:02X} code={code}")
    return MbResponse(rx_txid, rx_unit, fc, pdu)


def read_bits(sock: socket.socket, txid: int, unit: int, fc: int,
              address: int, quantity: int) -> list[int]:
    rsp = transact(sock, build_request(txid, unit, fc, address, quantity))
    if rsp.function_code != fc or len(rsp.pdu) < 2:
        raise RuntimeError("unexpected bit-read response")
    byte_count = rsp.pdu[1]
    data = rsp.pdu[2:2 + byte_count]
    if len(data) != byte_count:
        raise RuntimeError("truncated bit-read response")
    return [(data[i // 8] >> (i % 8)) & 1 for i in range(quantity)]


def read_registers(sock: socket.socket, txid: int, unit: int, fc: int,
                   address: int, quantity: int, signed: bool = False) -> list[int]:
    rsp = transact(sock, build_request(txid, unit, fc, address, quantity))
    if rsp.function_code != fc or len(rsp.pdu) < 2 or rsp.pdu[1] != quantity * 2:
        raise RuntimeError("unexpected register-read response")
    raw = rsp.pdu[2:]
    fmt = ">" + (("h" if signed else "H") * quantity)
    return list(struct.unpack(fmt, raw))


def write_single_register(sock: socket.socket, txid: int, unit: int,
                          address: int, signed_value: int) -> None:
    if not -32768 <= signed_value <= 32767:
        raise ValueError("signed register outside int16 range")
    word = signed_value & 0xFFFF
    rsp = transact(sock, build_request(txid, unit, 6, address, word))
    if rsp.function_code != 6 or rsp.pdu != struct.pack(">BHH", 6, address, word):
        raise RuntimeError("write-single-register echo mismatch")


def self_test() -> None:
    vectors = [
        (build_request(1, 1, 2, 0, 16), "000100000006010200000010", "U1 FC02 DI"),
        (build_request(2, 1, 4, 1024, 4), "000200000006010404000004", "U1 FC04 AI"),
        (build_request(3, 2, 2, 0, 16), "000300000006020200000010", "U2 FC02 DI"),
        (build_request(4, 2, 3, 0, 4), "000400000006020300000004", "U2 FC03 AO readback"),
        (build_request(5, 2, 4, 3072, 4), "00050000000602040c000004", "U2 FC04 AO status"),
        (build_request(6, 2, 6, 0, 132), "000600000006020600000084", "U2 FC06 +1.32 mA"),
    ]
    for request, expected_hex, label in vectors:
        expected = bytes.fromhex(expected_hex)
        if request != expected:
            raise AssertionError(f"{label} frame mismatch: {request.hex()} != {expected_hex}")

    signed = struct.unpack(">h", struct.pack(">H", 0xFF84))[0]
    if signed != -124:
        raise AssertionError("signed register conversion failed")
    if abs(132 / 100000.0 - 0.00132) > 1e-12:
        raise AssertionError("AO SI scaling invariant failed")
    print("STM32F407/LAN8720 Modbus contract self-test passed")


def run(args: argparse.Namespace) -> int:
    if args.self_test:
        self_test()
        return 0

    with socket.create_connection((args.host, args.port), timeout=args.timeout) as sock:
        sock.settimeout(args.timeout)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        txid = 1
        start = time.monotonic()
        cycles = 0
        while cycles < args.cycles:
            u1_di = read_bits(sock, txid, 1, 2, 0, 16); txid += 1
            u1_ai_mv = read_registers(sock, txid, 1, 4, 1024, 4, signed=True); txid += 1
            u2_di = read_bits(sock, txid, 2, 2, 0, 16); txid += 1
            u2_ao = read_registers(sock, txid, 2, 3, 0, 4, signed=True); txid += 1
            u2_status = read_registers(sock, txid, 2, 4, 3072, 4); txid += 1

            if args.write_ao is not None:
                command = int(round(args.write_ao * 100.0))
                if not -2000 <= command <= 2000:
                    raise ValueError("--write-ao must be within -20.00..+20.00 mA")
                write_single_register(sock, txid, 2, args.ao_channel - 1, command)
                txid += 1

            if not args.quiet:
                ao_ma = [v / 100.0 for v in u2_ao]
                print(f"cycle={cycles + 1} U1_DI={u1_di} U1_AI_mV={u1_ai_mv} "
                      f"U2_DI={u2_di} U2_AO_mA={ao_ma} U2_STATUS={u2_status}")
            cycles += 1
            target = start + cycles * (args.period_ms / 1000.0)
            delay = target - time.monotonic()
            if delay > 0:
                time.sleep(delay)

        elapsed = time.monotonic() - start
        request_count = cycles * (6 if args.write_ao is not None else 5)
        print(f"Completed {cycles} persistent-TCP cycles in {elapsed:.3f}s; "
              f"~{request_count / max(elapsed, 1e-9):.1f} requests/s")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="192.168.1.7", help="RHEL gateway IPv4 address (default: 192.168.1.7)")
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--period-ms", type=float, default=20.0)
    parser.add_argument("--cycles", type=int, default=10)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--write-ao", type=float, metavar="MA",
                        help="optional Unit-2 EL4074 command in mA")
    parser.add_argument("--ao-channel", type=int, choices=range(1, 5), default=1)
    parser.add_argument("--quiet", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.period_ms <= 0 or args.cycles <= 0:
        parser.error("--period-ms and --cycles must be positive")
    return args


if __name__ == "__main__":
    try:
        sys.exit(run(parse_args()))
    except (OSError, ValueError, RuntimeError, ConnectionError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
