#!/usr/bin/env python3
"""Command-line Modbus TCP client for the EK9500 gateway fixed map.

Author: Prathamesh Chipkar

This tool is intentionally stdlib-only. It supports the exact gateway map used
by the STM32 master: DI/DO, EL3074 AI, and EL4074 AO on Unit 1. Protocol addresses are zero-based internally;
channels in this CLI are one-based for technician use.
"""
from __future__ import annotations

import argparse
import socket
import struct
import sys
from typing import Iterable, List, Sequence

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 1502
TIMEOUT_S = 3.0
MAX_ADU = 260

FC_READ_COILS = 1
FC_READ_DISCRETE_INPUTS = 2
FC_READ_HOLDING_REGISTERS = 3
FC_READ_INPUT_REGISTERS = 4
FC_WRITE_SINGLE_COIL = 5
FC_WRITE_SINGLE_REGISTER = 6
FC_WRITE_MULTIPLE_COILS = 15
FC_WRITE_MULTIPLE_REGISTERS = 16

AI_RAW_BASE = 0
AI_ENGINEERING_BASE = 1024
AI_STATUS_BASE = 2048
AO_STATUS_BASE = 3072


def s16_to_u16(value: int) -> int:
    return value & 0xFFFF


def u16_to_s16(value: int) -> int:
    value &= 0xFFFF
    return value if value < 0x8000 else value - 0x10000


class ModbusClient:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.tid = 0
        self.sock: socket.socket | None = None

    def __enter__(self) -> "ModbusClient":
        self.sock = socket.create_connection((self.host, self.port), TIMEOUT_S)
        self.sock.settimeout(TIMEOUT_S)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:  # type: ignore[no-untyped-def]
        if self.sock is not None:
            self.sock.close()
            self.sock = None

    @staticmethod
    def _recv_exact(sock: socket.socket, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                raise RuntimeError("connection closed by server")
            data.extend(chunk)
        return bytes(data)

    def transact(self, unit: int, pdu: bytes) -> bytes:
        if self.sock is None:
            raise RuntimeError("client is not connected")
        self.tid = (self.tid + 1) & 0xFFFF or 1
        request = struct.pack(">HHHB", self.tid, 0, len(pdu) + 1, unit) + pdu
        self.sock.sendall(request)
        header = self._recv_exact(self.sock, 7)
        tid, pid, length, rx_unit = struct.unpack(">HHHB", header)
        if tid != self.tid or pid != 0 or rx_unit != unit:
            raise RuntimeError("invalid MBAP response")
        if not 2 <= length <= (MAX_ADU - 6):
            raise RuntimeError(f"invalid MBAP length {length}")
        body = self._recv_exact(self.sock, length - 1)
        if body[0] & 0x80:
            code = body[1] if len(body) > 1 else -1
            raise RuntimeError(f"Modbus exception FC=0x{body[0] & 0x7F:02X} code={code}")
        if body[0] != pdu[0]:
            raise RuntimeError("function-code mismatch")
        return body

    def read_bits(self, unit: int, fc: int, start: int, count: int) -> List[bool]:
        body = self.transact(unit, struct.pack(">BHH", fc, start, count))
        expected = (count + 7) // 8
        if len(body) != expected + 2 or body[1] != expected:
            raise RuntimeError("invalid bit-read response")
        return [bool((body[2 + i // 8] >> (i % 8)) & 1) for i in range(count)]

    def read_registers(self, unit: int, fc: int, start: int, count: int) -> List[int]:
        body = self.transact(unit, struct.pack(">BHH", fc, start, count))
        expected = count * 2
        if len(body) != expected + 2 or body[1] != expected:
            raise RuntimeError("invalid register-read response")
        return list(struct.unpack(">" + "H" * count, body[2:]))

    def write_single_coil(self, unit: int, address: int, state: bool) -> None:
        value = 0xFF00 if state else 0x0000
        body = self.transact(unit, struct.pack(">BHH", FC_WRITE_SINGLE_COIL, address, value))
        if body != struct.pack(">BHH", FC_WRITE_SINGLE_COIL, address, value):
            raise RuntimeError("invalid FC05 write echo")

    def write_multiple_coils(self, unit: int, start: int, values: Sequence[bool]) -> None:
        count = len(values)
        byte_count = (count + 7) // 8
        packed = bytearray(byte_count)
        for i, state in enumerate(values):
            if state:
                packed[i // 8] |= 1 << (i % 8)
        pdu = struct.pack(">BHHB", FC_WRITE_MULTIPLE_COILS, start, count, byte_count) + bytes(packed)
        body = self.transact(unit, pdu)
        if body != struct.pack(">BHH", FC_WRITE_MULTIPLE_COILS, start, count):
            raise RuntimeError("invalid FC15 write echo")

    def write_single_register(self, unit: int, address: int, value: int) -> None:
        word = s16_to_u16(value)
        body = self.transact(unit, struct.pack(">BHH", FC_WRITE_SINGLE_REGISTER, address, word))
        if body != struct.pack(">BHH", FC_WRITE_SINGLE_REGISTER, address, word):
            raise RuntimeError("invalid FC06 write echo")

    def write_multiple_registers(self, unit: int, start: int, values: Sequence[int]) -> None:
        words = [s16_to_u16(v) for v in values]
        payload = b"".join(struct.pack(">H", w) for w in words)
        pdu = struct.pack(">BHHB", FC_WRITE_MULTIPLE_REGISTERS, start, len(words), len(payload)) + payload
        body = self.transact(unit, pdu)
        if body != struct.pack(">BHH", FC_WRITE_MULTIPLE_REGISTERS, start, len(words)):
            raise RuntimeError("invalid FC16 write echo")


def channel_index(channel: int, max_channel: int) -> int:
    if not 1 <= channel <= max_channel:
        raise argparse.ArgumentTypeError(f"channel must be 1..{max_channel}")
    return channel - 1


def parse_bool(text: str) -> bool:
    t = text.strip().lower()
    if t in {"1", "on", "true", "high", "yes"}:
        return True
    if t in {"0", "off", "false", "low", "no"}:
        return False
    raise argparse.ArgumentTypeError("value must be on/off/high/low/1/0")


def print_bits(values: Sequence[bool], label: str) -> None:
    for i, state in enumerate(values, start=1):
        print(f"{label}{i:02d}= {'ON' if state else 'OFF'}")


def print_registers(values: Iterable[int], label: str, signed: bool = True) -> None:
    for i, word in enumerate(values, start=1):
        value = u16_to_s16(word) if signed else (word & 0xFFFF)
        print(f"{label}{i:02d}= {value} (0x{word & 0xFFFF:04X})")


def run(args: argparse.Namespace) -> int:
    with ModbusClient(args.host, args.port) as client:
        if args.command == "read-di":
            values = client.read_bits(args.unit, FC_READ_DISCRETE_INPUTS, 0, 16)
            print_bits(values, f"U{args.unit}_DI")
        elif args.command == "read-do":
            values = client.read_bits(args.unit, FC_READ_COILS, 0, 16)
            print_bits(values, f"U{args.unit}_DO")
        elif args.command == "write-do":
            client.write_single_coil(args.unit, channel_index(args.channel, 16), args.value)
            print(f"OK: U{args.unit} DO{args.channel:02d} = {'ON' if args.value else 'OFF'}")
        elif args.command == "write-do-mask":
            mask = int(args.mask, 0) & 0xFFFF
            states = [bool((mask >> i) & 1) for i in range(16)]
            client.write_multiple_coils(args.unit, 0, states)
            print(f"OK: U{args.unit} DO mask = 0x{mask:04X}")
        elif args.command == "read-ai-raw":
            print_registers(client.read_registers(args.unit, FC_READ_INPUT_REGISTERS, AI_RAW_BASE, 4), "AI")
        elif args.command == "read-ai":
            regs = client.read_registers(args.unit, FC_READ_INPUT_REGISTERS, AI_ENGINEERING_BASE, 4)
            for i, word in enumerate(regs, start=1):
                scaled = u16_to_s16(word)
                if args.profile == "voltage":
                    print(f"AI{i:02d}= {scaled:+d} mV = {scaled / 1000.0:+.3f} V")
                else:
                    print(f"AI{i:02d}= {scaled / 100.0:.2f} mA")
        elif args.command == "read-ai-status":
            print_registers(client.read_registers(args.unit, FC_READ_INPUT_REGISTERS, AI_STATUS_BASE, 4), "AI_STATUS", signed=False)
        elif args.command == "read-ao":
            regs = client.read_registers(1, FC_READ_HOLDING_REGISTERS, 0, 4)
            for i, word in enumerate(regs, start=1):
                scaled = u16_to_s16(word)
                if args.profile == "voltage":
                    print(f"AO{i:02d}= {scaled:+d} centi-V = {scaled / 100.0:+.2f} V")
                else:
                    print(f"AO{i:02d}= {scaled} centi-mA = {scaled / 100.0:.2f} mA")
        elif args.command == "write-ao-voltage":
            scaled = int(round(args.volts * 100.0))
            if not -1000 <= scaled <= 1000:
                raise ValueError("voltage must be within -10.00..+10.00 V")
            client.write_single_register(1, channel_index(args.channel, 4), scaled)
            print(f"OK: AO{args.channel:02d} = {args.volts:+.2f} V ({scaled} centi-V)")
        elif args.command == "write-ao-current":
            scaled = int(round(args.ma * 100.0))
            if args.profile == "4-20" and not 400 <= scaled <= 2000:
                raise ValueError("deprecated 4-20 mA mode requires 4.00..20.00 mA")
            if args.profile == "bipolar" and not -2000 <= scaled <= 2000:
                raise ValueError("bipolar current mode requires -20.00..+20.00 mA")
            client.write_single_register(1, channel_index(args.channel, 4), scaled)
            print(f"OK: AO{args.channel:02d} = {args.ma:.2f} mA ({scaled} centi-mA)")
        elif args.command == "write-ao-current-all":
            scaled = [int(round(v * 100.0)) for v in args.ma]
            for v in scaled:
                if args.profile == "4-20" and not 400 <= v <= 2000:
                    raise ValueError("deprecated 4-20 mA mode requires all values to be 4.00..20.00 mA")
                if args.profile == "bipolar" and not -2000 <= v <= 2000:
                    raise ValueError("bipolar current mode requires all values to be -20.00..+20.00 mA")
            client.write_multiple_registers(1, 0, scaled)
            print("OK: AO current values = " + ", ".join(f"{v:.2f} mA" for v in args.ma))
        elif args.command == "read-ao-status":
            print_registers(client.read_registers(1, FC_READ_INPUT_REGISTERS, AO_STATUS_BASE, 4), "AO_STATUS", signed=False)
        else:
            raise RuntimeError(f"unhandled command {args.command}")
    return 0


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST, help=f"gateway host, default {DEFAULT_HOST}")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"gateway port, default {DEFAULT_PORT}")
    sub = parser.add_subparsers(dest="command", required=True)

    for name in ("read-di", "read-do", "read-ai-raw", "read-ai", "read-ai-status"):
        p = sub.add_parser(name)
        p.add_argument("--unit", type=int, choices=(1,), default=1)
    sub.choices["read-ai"].add_argument("--profile", choices=("voltage", "current"), default="voltage")

    p = sub.add_parser("write-do")
    p.add_argument("--unit", type=int, choices=(1,), default=1)
    p.add_argument("--channel", type=int, required=True)
    p.add_argument("--value", type=parse_bool, required=True)

    p = sub.add_parser("write-do-mask")
    p.add_argument("--unit", type=int, choices=(1,), default=1)
    p.add_argument("--mask", required=True, help="16-bit mask, e.g. 0x0001 or 0x00FF")

    p = sub.add_parser("read-ao")
    p.add_argument("--profile", choices=("voltage", "current"), default="current")

    p = sub.add_parser("write-ao-voltage")
    p.add_argument("--channel", type=int, required=True)
    p.add_argument("--volts", type=float, required=True)

    p = sub.add_parser("write-ao-current")
    p.add_argument("--channel", type=int, required=True)
    p.add_argument("--ma", type=float, required=True)
    p.add_argument("--profile", choices=("bipolar", "4-20"), default="bipolar", help="current profile; default is bipolar +/-20 mA, 4-20 is deprecated compatibility")

    p = sub.add_parser("write-ao-current-all")
    p.add_argument("--ma", type=float, nargs=4, required=True, metavar=("AO1", "AO2", "AO3", "AO4"))
    p.add_argument("--profile", choices=("bipolar", "4-20"), default="bipolar", help="current profile; default is bipolar +/-20 mA, 4-20 is deprecated compatibility")

    sub.add_parser("read-ao-status")
    return parser.parse_args(argv)


if __name__ == "__main__":
    try:
        raise SystemExit(run(parse_args(sys.argv[1:])))
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
