"""Talk to the door firmware over the kit's on-board J-Link.

The firmware exposes a `door` struct (motor-fw/door.h). We look up its address
in the ELF, then read telemetry and write commands while the MCU keeps running,
the same way Infineon's Motor Suite GUI talks to the board.

    python door_link.py monitor                 # read only, motor never driven
    python door_link.py calibrate               # bare shaft: find which way + current turns it
    python door_link.py run --level 0.5 ...     # arm and resist opening
"""

import argparse
import os
import struct
import subprocess
import sys
import threading
import time
from dataclasses import dataclass, fields

import pylink

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ELF = os.path.join(
    HERE, "..", "motor-fw", "build", "APP_KIT_PSC3M5_CC2", "Release",
    "mtb-example-ce240614-motor-control-solutions.elf",
)
NM = os.path.expanduser("~/Applications/mtb-gcc-arm-eabi/14.2.1/gcc/bin/arm-none-eabi-nm")
JLINK_DEVICE = "PSC3xxF"  # attach without reset; the _tm variant resets into test mode

STATE_NAMES = {0: "DISARMED", 1: "IDLE", 2: "RESIST", 3: "COOLDOWN"}


@dataclass
class Door:
    # Same order and types as DOOR_t in motor-fw/door.h
    host_hb: int
    armed: int
    level: float
    i_take: float
    i_resist_max: float
    open_dir: int
    wind_sign: int
    fw_ms: int
    pos: int
    cps: float
    hall_uvw: int
    hall_errors: int
    state: int
    i_cmd: float


FORMAT = "<IIfffiiIifIIIf"
WORDS = struct.calcsize(FORMAT) // 4
OFFSET = {f.name: 4 * i for i, f in enumerate(fields(Door))}
FIELD_FMT = dict(zip(OFFSET, FORMAT[1:]))


def find_symbol(elf, name):
    out = subprocess.run([NM, elf], check=True, capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    sys.exit(f"symbol {name!r} not found in {elf}; rebuild motor-fw")


class DoorLink:
    def __init__(self, elf=DEFAULT_ELF):
        self.addr = find_symbol(elf, "door")
        self.jlink = pylink.JLink()
        self.jlink.open()
        self.jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
        self.jlink.connect(JLINK_DEVICE, speed=4000)
        self._lock = threading.Lock()
        self._hb_stop = threading.Event()
        self._hb_thread = None

    def read(self):
        with self._lock:
            words = self.jlink.memory_read32(self.addr, WORDS)
        return Door(*struct.unpack(FORMAT, struct.pack(f"<{WORDS}I", *words)))

    def write(self, **values):
        with self._lock:
            for name, value in values.items():
                raw = struct.unpack("<I", struct.pack("<" + FIELD_FMT[name], value))[0]
                self.jlink.memory_write32(self.addr + OFFSET[name], [raw])

    def start_heartbeat(self, period=0.05):
        def beat():
            hb = self.read().host_hb
            while not self._hb_stop.is_set():
                hb = (hb + 1) & 0xFFFFFFFF
                self.write(host_hb=hb)
                time.sleep(period)

        self._hb_thread = threading.Thread(target=beat, daemon=True)
        self._hb_thread.start()

    def close(self):
        self._hb_stop.set()
        if self._hb_thread:
            self._hb_thread.join(timeout=1)
        try:
            self.write(armed=0)
        finally:
            self.jlink.close()


def fmt(d):
    return (
        f"t={d.fw_ms/1000:8.2f}s  {STATE_NAMES.get(d.state, d.state):9s} "
        f"hall={d.hall_uvw:03b} pos={d.pos:6d} ({d.pos/24:+6.2f} rev) "
        f"cps={d.cps:+7.1f} i={d.i_cmd:4.2f}A err={d.hall_errors}"
    )


def monitor(link, args):
    link.write(armed=0)
    print("monitor: drive stays off. Turn the shaft by hand; pos should change by 24 per turn. Ctrl+C to stop.")
    while True:
        print(fmt(link.read()))
        time.sleep(0.1)


def calibrate(link, args):
    # With nothing on the shaft, a small positive current spins it one way.
    # That way is "winding" when wind_sign = +1, so opening is the other way.
    link.write(armed=0, level=0.0, i_take=args.current, i_resist_max=args.current, open_dir=+1, wind_sign=+1)
    start = link.read()
    link.start_heartbeat()
    link.write(armed=1)
    time.sleep(args.seconds)
    link.write(armed=0)
    time.sleep(0.3)
    end = link.read()
    moved = end.pos - start.pos
    print(f"pos moved {moved:+d} counts with positive current")
    if moved == 0:
        print("shaft did not move: check the 24 V supply and the motor wires")
        return
    wind_dir = 1 if moved > 0 else -1
    print(f"-> with --wind-sign 1, use --open-dir {-wind_dir}")


def run(link, args):
    link.write(
        armed=0, level=args.level, i_take=args.i_take, i_resist_max=args.i_max,
        open_dir=args.open_dir, wind_sign=args.wind_sign,
    )
    link.start_heartbeat()
    link.write(armed=1)
    print(f"armed: level={args.level} i_take={args.i_take}A i_max={args.i_max}A. Ctrl+C to disarm.")
    while True:
        print(fmt(link.read()))
        time.sleep(0.1)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", default=DEFAULT_ELF)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("monitor")
    c = sub.add_parser("calibrate")
    c.add_argument("--current", type=float, default=0.2)
    c.add_argument("--seconds", type=float, default=0.15)
    r = sub.add_parser("run")
    r.add_argument("--level", type=float, default=0.5)
    r.add_argument("--i-take", type=float, default=0.3)
    r.add_argument("--i-max", type=float, default=2.0)
    r.add_argument("--open-dir", type=int, choices=(-1, 1), required=True)
    r.add_argument("--wind-sign", type=int, choices=(-1, 1), default=1)
    args = p.parse_args()

    link = DoorLink(args.elf)
    try:
        {"monitor": monitor, "calibrate": calibrate, "run": run}[args.cmd](link, args)
    except KeyboardInterrupt:
        pass
    finally:
        link.close()
        print("disarmed")


if __name__ == "__main__":
    main()
