#!/usr/bin/env python3
"""Read-only live monitor for the v1.8.0 single EK9500 Modbus TCP gateway.

Author: Prathamesh Chipkar

The monitor deliberately uses only the gateway's fixed Modbus map. It does not
provide arbitrary reads or any write function. Unit 1 EL4074 AO readback is
shown according to the canonical gateway command representation:

    signed register count * 0.01 mA/count

Therefore Holding Register 40001 value 132 is displayed as 1.320 mA and its
internal EtherNet/IP REAL32 equivalent is 0.00132 A. The REAL32 PDO is not
exposed as eight Modbus Holding Registers.
"""
from __future__ import annotations

import argparse
import queue
import socket
import struct
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from datetime import datetime
from tkinter import messagebox, ttk
from typing import List, Optional, Sequence, Tuple

APP_TITLE = "EK9500 Maritime I/O Monitor v1.8.0"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 1502
DEFAULT_TIMEOUT_S = 2.0
DEFAULT_INTERVAL_S = 0.5
MAX_MODBUS_ADU = 260

FC01 = 0x01
FC02 = 0x02
FC03 = 0x03
FC04 = 0x04


class ModbusError(RuntimeError):
    """Base error for transport, protocol, or Modbus exception failures."""


def u16_to_s16(value: int) -> int:
    """Return the signed int16 interpretation of one Modbus register word."""
    value &= 0xFFFF
    return value if value < 0x8000 else value - 0x10000


def ao_command_to_ma(word: int) -> float:
    """Decode the v1.2.4+ AO command format (0.01 mA per signed count)."""
    command = u16_to_s16(word)
    if not -2000 <= command <= 2000:
        raise ValueError(f"AO command {command} outside -2000..2000")
    return command / 100.0


def ao_command_to_amperes(word: int) -> float:
    """Decode the Modbus command to the SI value encoded in EL4074 REAL32."""
    return ao_command_to_ma(word) / 1000.0


@dataclass(frozen=True)
class PollBlock:
    key: str
    unit: int
    fc: int
    start: int
    count: int
    group: str
    channels: str
    format_kind: str


BLOCKS: Tuple[PollBlock, ...] = (
    PollBlock("u1_do", 1, FC01, 0, 16, "EL2409 Digital Outputs", "DO", "bit"),
    PollBlock("u1_di", 1, FC02, 0, 16, "EL1409 Digital Inputs", "DI", "bit"),
    PollBlock("u1_ai_raw", 1, FC04, 0, 4, "EL3074 Analog Inputs / Raw", "AI", "signed"),
    PollBlock("u1_ai_eng", 1, FC04, 1024, 4, "EL3074 Analog Inputs / Engineering", "AI", "ai_mv"),
    PollBlock("u1_ao", 1, FC03, 0, 4, "EL4074 Analog Outputs / Command", "AO", "ao_current"),
)


class ModbusTcpClient:
    """Minimal persistent Modbus TCP read client with MBAP validation."""

    def __init__(self, host: str, port: int, timeout_s: float) -> None:
        self.host = host
        self.port = port
        self.timeout_s = timeout_s
        self.sock: Optional[socket.socket] = None
        self.tid = 0

    def close(self) -> None:
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None

    def _connect(self) -> socket.socket:
        if self.sock is None:
            try:
                self.sock = socket.create_connection((self.host, self.port), self.timeout_s)
                self.sock.settimeout(self.timeout_s)
            except OSError as exc:
                self.sock = None
                raise ModbusError(f"connect {self.host}:{self.port}: {exc}") from exc
        return self.sock

    @staticmethod
    def _recv_exact(sock: socket.socket, size: int) -> bytes:
        data = bytearray()
        while len(data) < size:
            chunk = sock.recv(size - len(data))
            if not chunk:
                raise ModbusError("connection closed by server")
            data.extend(chunk)
        return bytes(data)

    def _exchange(self, unit: int, pdu: bytes) -> bytes:
        sock = self._connect()
        self.tid = (self.tid + 1) & 0xFFFF or 1
        request = struct.pack(">HHHB", self.tid, 0, len(pdu) + 1, unit) + pdu
        try:
            sock.sendall(request)
            header = self._recv_exact(sock, 7)
            tid, pid, length, rx_unit = struct.unpack(">HHHB", header)
            if tid != self.tid or pid != 0 or rx_unit != unit:
                raise ModbusError("invalid MBAP response")
            if not 2 <= length <= (MAX_MODBUS_ADU - 6):
                raise ModbusError(f"invalid MBAP length {length}")
            body = self._recv_exact(sock, length - 1)
        except (OSError, ModbusError) as exc:
            self.close()
            if isinstance(exc, ModbusError):
                raise
            raise ModbusError(str(exc)) from exc
        if body[0] & 0x80:
            code = body[1] if len(body) > 1 else -1
            raise ModbusError(f"Modbus exception FC=0x{body[0] & 0x7F:02X} code={code}")
        if body[0] != pdu[0]:
            raise ModbusError("function-code mismatch")
        return body

    def poll(self, block: PollBlock) -> Sequence[object]:
        body = self._exchange(block.unit, struct.pack(">BHH", block.fc, block.start, block.count))
        if block.fc in (FC01, FC02):
            expected = (block.count + 7) // 8
            if len(body) != expected + 2 or body[1] != expected:
                raise ModbusError("invalid bit-read response length")
            return [bool((body[2 + i // 8] >> (i % 8)) & 1) for i in range(block.count)]
        expected = block.count * 2
        if len(body) != expected + 2 or body[1] != expected:
            raise ModbusError("invalid register-read response length")
        return struct.unpack(">" + "H" * block.count, body[2:])


def format_value(kind: str, raw: object) -> Tuple[str, str]:
    """Return (engineering display, raw diagnostic display)."""
    if kind == "bit":
        state = bool(raw)
        return ("ON" if state else "OFF", "1" if state else "0")
    word = int(raw) & 0xFFFF
    signed = u16_to_s16(word)
    if kind == "signed":
        return (str(signed), f"0x{word:04X}")
    if kind == "ai_mv":
        return (f"{signed:+.3f} mV ({signed / 1000.0:+.3f} V)", f"0x{word:04X}")
    if kind == "ao_current":
        try:
            ma = ao_command_to_ma(word)
            amps = ma / 1000.0
            return (f"{ma:+.3f} mA ({amps:+.5f} A REAL32)", f"{signed} / 0x{word:04X}")
        except ValueError as exc:
            return (f"OUT OF RANGE ({signed})", str(exc))
    return (str(word), f"0x{word:04X}")


class PollWorker(threading.Thread):
    def __init__(self, client: ModbusTcpClient, interval_s: float,
                 events: "queue.Queue[Tuple[str, object]]", stop: threading.Event) -> None:
        super().__init__(daemon=True)
        self.client = client
        self.interval_s = interval_s
        self.events = events
        self.stop_event = stop

    def run(self) -> None:
        try:
            while not self.stop_event.is_set():
                for block in BLOCKS:
                    if self.stop_event.is_set():
                        break
                    try:
                        self.events.put(("data", (block, self.client.poll(block), datetime.now().strftime("%H:%M:%S.%f")[:-3])))
                    except Exception as exc:  # GUI boundary: surface per-block communication faults
                        self.events.put(("error", (block, str(exc), datetime.now().strftime("%H:%M:%S.%f")[:-3])))
                self.stop_event.wait(self.interval_s)
        finally:
            self.client.close()
            self.events.put(("stopped", None))


class Monitor(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title(APP_TITLE)
        self.geometry("1180x720")
        self.events: "queue.Queue[Tuple[str, object]]" = queue.Queue()
        self.stop_event = threading.Event()
        self.worker: Optional[PollWorker] = None
        self.host = tk.StringVar(value=DEFAULT_HOST)
        self.port = tk.StringVar(value=str(DEFAULT_PORT))
        self.interval = tk.StringVar(value=str(DEFAULT_INTERVAL_S))
        self.status = tk.StringVar(value="Stopped")
        self.trees: dict[int, ttk.Treeview] = {}
        self._build_ui()
        self.after(100, self._drain)
        self.protocol("WM_DELETE_WINDOW", self._close)

    def _build_ui(self) -> None:
        top = ttk.Frame(self, padding=8)
        top.pack(fill=tk.X)
        for text, var, width in (("Host", self.host, 18), ("Port", self.port, 7), ("Interval s", self.interval, 7)):
            ttk.Label(top, text=text).pack(side=tk.LEFT, padx=(0, 4))
            ttk.Entry(top, textvariable=var, width=width).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(top, text="Start", command=self.start_polling).pack(side=tk.LEFT, padx=4)
        ttk.Button(top, text="Stop", command=self.stop_polling).pack(side=tk.LEFT, padx=4)
        ttk.Label(top, textvariable=self.status).pack(side=tk.LEFT, padx=18)

        notebook = ttk.Notebook(self)
        notebook.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)
        for unit, title in ((1, "EK9500 192.168.1.1 — EL1409 DI / EL2409 DO / EL3074 AI / EL4074 AO"),):
            frame = ttk.Frame(notebook)
            notebook.add(frame, text="Beckhoff I/O Station")
            ttk.Label(frame, text=title).pack(anchor=tk.W, padx=6, pady=6)
            tree = ttk.Treeview(frame, columns=("group", "reference", "channel", "value", "raw", "updated"), show="headings")
            for col, heading, width in (("group", "Group", 260), ("reference", "Modbus", 110), ("channel", "Channel", 90), ("value", "Engineering", 300), ("raw", "Raw", 160), ("updated", "Updated", 110)):
                tree.heading(col, text=heading); tree.column(col, width=width, anchor=tk.W)
            tree.pack(fill=tk.BOTH, expand=True, padx=6, pady=6)
            self.trees[unit] = tree
        for block in BLOCKS:
            tree = self.trees[block.unit]
            base = {FC01: 1, FC02: 10001, FC03: 40001, FC04: 30001}[block.fc]
            for i in range(block.count):
                tree.insert("", tk.END, iid=f"{block.key}:{i}", values=(block.group, f"{base + block.start + i:05d}", f"{block.channels}{i+1}", "--", "--", "Never"))
        ttk.Label(self, padding=(8, 0, 8, 8), text="Read-only fixed map. Unit 1: 40001–40004 are signed 0.01 mA/count commands; 132 = 1.320 mA = 0.00132 A in the EL4074 REAL32 PDO. Status blocks are intentionally omitted from this commissioning view.").pack(fill=tk.X)

    def start_polling(self) -> None:
        if self.worker is not None and self.worker.is_alive():
            return
        try:
            port = int(self.port.get()); interval = float(self.interval.get())
            if not 1 <= port <= 65535 or interval < 0.1:
                raise ValueError
        except ValueError:
            messagebox.showerror(APP_TITLE, "Port must be 1..65535 and interval must be >= 0.1 s")
            return
        self.stop_event = threading.Event()
        self.worker = PollWorker(ModbusTcpClient(self.host.get().strip(), port, DEFAULT_TIMEOUT_S), interval, self.events, self.stop_event)
        self.status.set(f"LIVE {self.host.get().strip()}:{port}")
        self.worker.start()

    def stop_polling(self) -> None:
        self.stop_event.set()

    def _drain(self) -> None:
        try:
            while True:
                event, payload = self.events.get_nowait()
                if event in ("data", "error"):
                    block, content, timestamp = payload  # type: ignore[misc]
                    tree = self.trees[block.unit]
                    for i in range(block.count):
                        values = list(tree.item(f"{block.key}:{i}", "values"))
                        if event == "data":
                            values[3], values[4] = format_value(block.format_kind, content[i])
                        else:
                            values[3], values[4] = "ERROR", content
                        values[5] = timestamp
                        tree.item(f"{block.key}:{i}", values=values)
                elif event == "stopped":
                    self.status.set("Stopped")
        except queue.Empty:
            pass
        self.after(100, self._drain)

    def _close(self) -> None:
        self.stop_event.set()
        self.destroy()


def self_test() -> int:
    assert u16_to_s16(0xFFFF) == -1
    assert abs(ao_command_to_ma(132) - 1.32) < 1e-12
    assert abs(ao_command_to_amperes(132) - 0.00132) < 1e-12
    assert abs(ao_command_to_ma((1 << 16) - 500) + 5.0) < 1e-12
    try:
        ao_command_to_ma(2001)
    except ValueError:
        pass
    else:
        raise AssertionError("out-of-range AO command was accepted")
    print("Monitor self-test: PASS (132 -> 1.320 mA -> 0.00132 A)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true", help="run conversion checks without opening the GUI")
    args = parser.parse_args()
    if args.self_test:
        return self_test()
    Monitor().mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
