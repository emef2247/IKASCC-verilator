#!/usr/bin/env python3
"""Generate the synthetic public fixtures: tests/vgm/scc_scale.vgm and
tests/csv/scc_scale.vgm.csv.

One SCC channel plays an ascending C major scale with a sine-like
32-byte waveform. The VGM uses the K051649 register-select protocol;
the CSV is the same performance translated to physical bus writes
(mapper enable first), so both inputs must render identically.
"""
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLOCK = 3579545
NOTES = [261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25]
NOTE_SAMPLES = 6615  # 0.15 s at 44100 Hz
TAIL_SAMPLES = 4410  # 0.1 s


def period(freq):
    return round(CLOCK / (32.0 * freq)) - 1


def wave_bytes():
    return bytes(round(127 * math.sin(2 * math.pi * i / 32)) & 0xFF for i in range(32))


class VgmEvents:
    """Logical K051649 writes as (sample, port, data) plus the bus view."""

    def __init__(self):
        self.vgm = bytearray()
        self.bus = []  # (sample, abhi, ablo, data)
        self.sample = 0
        self.cur_reg = 0

    def wait(self, n):
        while n > 0:
            if n >= 65535:
                self.vgm += b"\x61\xff\xff"
                n -= 65535
            elif n == 735:
                self.vgm += b"\x62"
                n = 0
            elif n == 882:
                self.vgm += b"\x63"
                n = 0
            elif n <= 16:
                self.vgm.append(0x6F + n)
                n = 0
            else:
                self.vgm += b"\x61" + struct.pack("<H", n)
                n = 0
        self.sample += 0  # caller advances self.sample explicitly

    def advance(self, n):
        self.wait(n)
        self.sample += n

    def write_group(self, group, reg, data, bus_ablo):
        # One 0xD2 command carries port (group), register, and data.
        self.vgm += bytes([0xD2, group, reg, data])
        self.bus.append((self.sample, 0x13, bus_ablo, data))

    def waveform(self, addr, data):
        bus = addr if addr < 0x60 else 0x60 + (addr & 0x1F)
        self.write_group(0, addr, data, bus)

    def frequency(self, reg, data):
        self.write_group(1, reg, data, 0x80 + (reg & 0x0F))

    def volume(self, ch, data):
        self.write_group(2, ch, data, 0x8A + ch)

    def key(self, data):
        self.write_group(3, 0, data, 0x8F)


def build():
    ev = VgmEvents()
    ev.bus.append((0, 0x12, 0x00, 0x3F))  # mapper BR2 := 0x3F (SCC mode)
    for i, b in enumerate(wave_bytes()):
        ev.waveform(i, b)
    ev.volume(0, 15)
    for freq in NOTES:
        tp = period(freq)
        ev.frequency(0, tp & 0xFF)
        ev.frequency(1, (tp >> 8) & 0x0F)
        ev.key(0x01)
        ev.advance(NOTE_SAMPLES)
        ev.key(0x00)
    ev.advance(TAIL_SAMPLES)
    total = ev.sample
    ev.vgm += b"\x66"

    header = bytearray(0x100)
    header[0:4] = b"Vgm "
    struct.pack_into("<I", header, 0x08, 0x00000171)          # version 1.71
    struct.pack_into("<I", header, 0x18, total)               # total samples
    struct.pack_into("<I", header, 0x34, 0x100 - 0x34)        # data offset
    struct.pack_into("<I", header, 0xD0, CLOCK)               # K051649 clock
    struct.pack_into("<I", header, 0x04, 0x100 + len(ev.vgm) - 4)
    vgm = bytes(header) + bytes(ev.vgm)

    lines = ["# clock_hz=%d" % CLOCK, "delay,abhi,ablo,data"]
    last = 0
    for sample, abhi, ablo, data in ev.bus:
        lines.append("%d,%02X,%02X,%02X" % (sample - last, abhi, ablo, data))
        last = sample
    if total > last:
        lines.append("%d,wait,00,00" % (total - last))
    csv = "\n".join(lines) + "\n"
    return vgm, csv


def main():
    vgm, csv = build()
    (ROOT / "tests/vgm/scc_scale.vgm").write_bytes(vgm)
    (ROOT / "tests/csv/scc_scale.vgm.csv").write_text(csv)
    print("wrote tests/vgm/scc_scale.vgm (%d bytes) and tests/csv/scc_scale.vgm.csv" % len(vgm))


if __name__ == "__main__":
    main()
