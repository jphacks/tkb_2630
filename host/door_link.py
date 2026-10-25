"""Talk to the door firmware over the kit's on-board J-Link.

The firmware exposes a `door` struct (motor-fw/door.h). We look up its address
in the ELF, then read telemetry and write commands while the MCU keeps running,
the same way Infineon's Motor Suite GUI talks to the board.

    python door_link.py monitor                 # read only, motor never driven
    python door_link.py calibrate               # bare shaft: find which way + current turns it
    python door_link.py run --level 0.5 ...     # arm and resist opening
"""

import argparse
import csv
import json
import os
import struct
import subprocess
import sys
import threading
import time
import unicodedata
from dataclasses import dataclass, fields

import pylink

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ELF = os.path.join(
    HERE, "..", "motor-fw", "build", "APP_KIT_PSC3M5_CC2", "Release",
    "mtb-example-ce240614-motor-control-solutions.elf",
)
NM = os.path.expanduser("~/Applications/mtb-gcc-arm-eabi/14.2.1/gcc/bin/arm-none-eabi-nm")
JLINK_DEVICE = "PSC3xxF"  # attach without reset; the _tm variant resets into test mode
# The motor kit's on-board J-Link, picked by serial number: the magnetic sensor kit has the
# same "J-Link Lite-XMC4200", so with both plugged in the first one found may be the wrong
# board. Set DOOR_JLINK_SERIAL for another kit; set it empty to take whichever J-Link is there.
JLINK_SERIAL = os.environ.get("DOOR_JLINK_SERIAL", "591234791")
CONTROL_FILE = os.path.join(HERE, "control.json")  # e.g. {"level": 0.5}; read by `run` when it changes
LOG_DIR = os.path.join(HERE, "logs")
MOUNT_DIR = os.path.join(HERE, "mounts")

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
    iq_fb: float
    iq_ref: float
    vdc: float
    lib_state: int
    lib_flags: int
    fault_sw: int
    free_counts: int
    ramp_counts: int
    home_pos: int
    open_dist: int
    k_damp: float
    buzz_amp: float
    buzz_hz: float
    buzz_ms: int
    buzz_req: int
    buzz_min_level: float
    full_counts: int
    vel_cps: float
    buzz_left_ms: int
    ev_active: int
    ev_count: int
    ev_energy_mj: float
    last_energy_mj: float
    last_peak_iq: float
    last_max_dist: int
    last_duration_ms: int
    last_gave_up: int
    open_window_ms: int
    hold_spring: int
    close_cps_max: float
    relent_delay_ms: int
    relent_ms: int
    relent_floor: float
    recover_ms: int
    strength: float
    release_counts: int


FORMAT = "<IIfffiiIifIIIffffIIIiiii" "fffIIfi" "fIIIfffiII" "IIf" "IIfIf" "i"

# Bit order of FAULT_FLAGS_t.sw in motor-ctrl-lib (CTRL_METHOD_RFO)
FAULT_NAMES = ["oc", "ot_ps", "ov_vdc", "uv_vdc", "os", "hall", "params", "brk", "em_stop",
               "encoder", "phase_loss", "offset_oor", "flux", "stall_detection", "profiler"]


def fault_names(bits):
    return ",".join(n for i, n in enumerate(FAULT_NAMES) if bits >> i & 1)


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
        self._lock = threading.Lock()
        self._hb_stop = threading.Event()
        self._hb_thread = None
        self._attach()

    def _attach(self):
        self.jlink.open(serial_no=int(JLINK_SERIAL) if JLINK_SERIAL else None)
        self.jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
        self.jlink.connect(JLINK_DEVICE, speed=4000)

    def reconnect(self):
        """After the board lost power or USB: drop the old session and attach again."""
        self.stop_heartbeat()
        with self._lock:
            try:
                self.jlink.close()
            except pylink.errors.JLinkException:
                pass
            self._attach()

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
        # If the link drops, the beat just stops: the firmware disarms on its own
        # and the caller reconnects.
        def beat():
            try:
                hb = self.read().host_hb
                while not self._hb_stop.is_set():
                    hb = (hb + 1) & 0xFFFFFFFF
                    self.write(host_hb=hb)
                    time.sleep(period)
            except pylink.errors.JLinkException:
                pass

        self._hb_stop.clear()
        self._hb_thread = threading.Thread(target=beat, daemon=True)
        self._hb_thread.start()

    def stop_heartbeat(self):
        self._hb_stop.set()
        if self._hb_thread:
            self._hb_thread.join(timeout=1)
            self._hb_thread = None

    def close(self):
        self.stop_heartbeat()
        try:
            self.write(armed=0)
        except pylink.errors.JLinkException:
            pass  # board already gone: it disarms itself without a heartbeat
        finally:
            self.jlink.close()


def fmt(d):
    return (
        f"t={d.fw_ms/1000:8.2f}s  {STATE_NAMES.get(d.state, d.state):9s} "
        f"hall={d.hall_uvw:03b} pos={d.pos:6d} ({d.pos/24:+6.2f} rev) "
        f"open={d.open_dist:4d} v={d.vel_cps:+6.0f} i={d.i_cmd:4.2f}A iq={d.iq_fb:+5.2f}A "
        f"{'CL' if d.lib_flags & 1 else ('FAULT' if d.lib_flags & 2 else f'lib{d.lib_state}')} "
        f"lv={d.level:.2f} s={d.strength:.2f} err={d.hall_errors}"
        + (f" E={d.ev_energy_mj / 1000:.2f}J" if d.ev_active else "")
        + (" BUZZ" if d.buzz_left_ms else "")
        + (f" faults=[{fault_names(d.fault_sw)}]" if d.fault_sw else "")
    )


def fmt_event(d):
    return (
        f"opening #{d.ev_count}: {d.last_energy_mj / 1000:.2f} J, peak {d.last_peak_iq:.2f} A, "
        f"{d.last_max_dist} counts in {d.last_duration_ms / 1000:.1f} s"
        + (" -> gave up" if d.last_gave_up else " -> opened")
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
    # The drive spends 0.25 s charging bootstrap capacitors before current flows,
    # so wait for the shaft to actually move, then cut it after a few counts.
    link.write(armed=0, level=0.0, i_take=args.current, i_resist_max=args.current, open_dir=+1, wind_sign=+1)
    start = link.read()
    link.start_heartbeat()
    link.write(armed=1)
    deadline = time.time() + args.seconds
    while time.time() < deadline and abs(link.read().pos - start.pos) < 3:
        time.sleep(0.005)
    link.write(armed=0)
    time.sleep(0.5)
    end = link.read()
    moved = end.pos - start.pos
    print(f"pos moved {moved:+d} counts with positive current")
    if moved == 0:
        print("shaft did not move: check the 24 V supply and the motor wires")
        return
    wind_dir = 1 if moved > 0 else -1
    print(f"-> with --wind-sign 1, use --open-dir {-wind_dir}")


def clamp(x, lo, hi):
    return min(max(x, lo), hi)


# Host setting -> (DOOR_t field, upper limit, is integer)
SETTINGS = {
    "level": ("level", 1.0, False),
    "take": ("i_take", 3.0, False),
    "max": ("i_resist_max", 3.0, False),
    "free": ("free_counts", 2400, True),
    "ramp": ("ramp_counts", 2400, True),
    "damp": ("k_damp", 0.5, False),
    "buzzamp": ("buzz_amp", 1.0, False),
    "buzzhz": ("buzz_hz", 100.0, False),
    "buzzms": ("buzz_ms", 1000, True),
    "buzzlevel": ("buzz_min_level", 1.1, False),  # above 1 never buzzes on opening
    "full": ("full_counts", 2400, True),
    "window": ("open_window_ms", 2000, True),
    "hold": ("hold_spring", 1, True),
    "closecps": ("close_cps_max", 1000.0, False),
    "relentdelay": ("relent_delay_ms", 10000, True),
    "relent": ("relent_ms", 20000, True),  # 0: never give in
    "floor": ("relent_floor", 1.0, False),
    "recover": ("recover_ms", 20000, True),
    "release": ("release_counts", 240, True),  # move back this far while resisting = let go
}


def load_mounts():
    """One JSON per way of mounting the motor (host/mounts/*.json): starting
    settings, door direction and the day/night/off feel presets."""
    mounts = {}
    for name in sorted(os.listdir(MOUNT_DIR)):
        if not name.endswith(".json"):
            continue
        with open(os.path.join(MOUNT_DIR, name)) as f:
            mount = json.load(f)
        for values in [mount["settings"], *mount["profiles"].values()]:
            unknown = set(values) - set(SETTINGS) - {"level"}
            if unknown:
                sys.exit(f"{name}: unknown settings {sorted(unknown)}")
        mounts[name[:-len(".json")]] = mount
    return mounts


MOUNTS = load_mounts()

HELP = ("type 0-1 for a level; 'max 2', 'take 0.3', 'free 36', 'ramp 24', 'damp 0.01', "
        "'buzzamp 0.6', 'buzzhz 40', 'buzzms 200', 'buzzlevel 0.5', 'full 72', 'window 120', "
        "'hold 1', 'closecps 60', 'relentdelay 1000', 'relent 2000', 'floor 0', 'recover 1500', 'release 3'; "
        "'buzz' to buzz now; 'profile day|night|off'; q to stop")


def run(link, args):
    mount = MOUNTS[args.mount]
    profiles = mount["profiles"]
    settings = dict(mount["settings"], level=args.level)
    for key in ("take", "max", "free", "ramp", "damp", "full"):
        value = getattr(args, key)
        if value is not None:  # explicit flags override the mounting's file
            settings[key] = value
    open_dir = args.open_dir if args.open_dir is not None else mount["open_dir"]
    wind_sign = args.wind_sign if args.wind_sign is not None else mount["wind_sign"]
    print(f"mount: {args.mount} (open_dir={open_dir}, wind_sign={wind_sign})")

    def apply(**changes):
        for key, value in changes.items():
            _, hi, is_int = SETTINGS[key]
            value = clamp(float(value), 0.0, hi)
            settings[key] = int(value) if is_int else value
        link.write(**{SETTINGS[k][0]: v for k, v in settings.items()})
        print("-> " + " ".join(f"{k}={v:g}" for k, v in settings.items()) + "  (counts: 24 per turn)")

    def buzz():
        link.write(buzz_req=(link.read().buzz_req + 1) & 0xFFFFFFFF)

    def arm():
        link.write(armed=0, open_dir=open_dir, wind_sign=wind_sign)
        apply()
        link.start_heartbeat()
        link.write(armed=1)

    arm()
    print("armed. " + HELP)
    print(f"(with a Japanese IME on, Enter only confirms the text: press it twice) or edit {CONTROL_FILE}")

    stop = threading.Event()

    def read_commands():
        for line in sys.stdin:
            words = unicodedata.normalize("NFKC", line).strip().lower().split()
            if words == ["q"]:
                break
            try:
                if words == ["buzz"]:
                    buzz()
                elif len(words) == 2 and words[0] == "profile" and words[1] in profiles:
                    apply(**profiles[words[1]])
                elif len(words) == 1:
                    apply(level=words[0])
                elif len(words) == 2 and words[0] in SETTINGS:
                    apply(**{words[0]: words[1]})
                elif words:
                    print("? " + HELP)
            except ValueError:
                print("? " + HELP)
            except pylink.errors.JLinkException:
                print("! board not reachable right now; the setting is applied after reconnecting")
        stop.set()

    threading.Thread(target=read_commands, daemon=True).start()

    def control_mtime():
        try:
            return os.path.getmtime(CONTROL_FILE)
        except OSError:
            return None

    seen_mtime = control_mtime()  # ignore whatever an earlier run left there
    os.makedirs(LOG_DIR, exist_ok=True)
    log_path = os.path.join(LOG_DIR, time.strftime("run-%Y%m%d-%H%M%S.csv"))
    events_path = os.path.join(LOG_DIR, "events.csv")  # one row per opening, kept across runs for Think
    new_events_file = not os.path.exists(events_path)
    with open(log_path, "w", newline="") as f, open(events_path, "a", newline="") as ef:
        log = csv.writer(f)
        log.writerow(["wall_s", "fw_ms", "state", "pos", "cps", "i_cmd", "level",
                      "iq_fb", "iq_ref", "vdc", "lib_state", "lib_flags", "faults", "open_dist", "home_pos",
                      "vel_cps", "ev_energy_mj", "buzz_left_ms", "strength"])
        events = csv.writer(ef)
        if new_events_file:
            events.writerow(["wall_s", "local_time", "mount", "energy_mj", "peak_iq", "max_dist", "duration_ms",
                             "gave_up", "level", "max", "damp", "free", "ramp"])
        seen_events = link.read().ev_count
        n = 0
        while not stop.is_set():
            try:
                d = link.read()
            except pylink.errors.JLinkException as e:
                # Power or USB dropped. Without the heartbeat the firmware has already
                # disarmed, so just keep trying, then send the settings and arm again.
                print(f"! lost the board ({e}); retrying every second. Check the 24 V adapter and USB.")
                while not stop.is_set():
                    time.sleep(1.0)
                    try:
                        link.reconnect()
                        arm()
                        seen_events = link.read().ev_count
                        print("reconnected and armed again")
                        break
                    except pylink.errors.JLinkException:
                        continue
                continue
            log.writerow([f"{time.time():.3f}", d.fw_ms, STATE_NAMES.get(d.state, d.state),
                          d.pos, f"{d.cps:.1f}", f"{d.i_cmd:.3f}", f"{d.level:.2f}",
                          f"{d.iq_fb:.3f}", f"{d.iq_ref:.3f}", f"{d.vdc:.2f}", d.lib_state, d.lib_flags,
                          fault_names(d.fault_sw), d.open_dist, d.home_pos,
                          f"{d.vel_cps:.1f}", f"{d.ev_energy_mj:.1f}", d.buzz_left_ms, f"{d.strength:.3f}"])
            if d.ev_count != seen_events:
                seen_events = d.ev_count
                events.writerow([f"{time.time():.3f}", time.strftime("%Y-%m-%d %H:%M:%S"), args.mount,
                                 f"{d.last_energy_mj:.1f}", f"{d.last_peak_iq:.3f}", d.last_max_dist,
                                 d.last_duration_ms, d.last_gave_up, f"{settings['level']:g}",
                                 f"{settings['max']:g}", f"{settings['damp']:g}", settings["free"], settings["ramp"]])
                ef.flush()
                print(fmt_event(d))
            if n % 4 == 0:
                print(fmt(d))
            n += 1

            mtime = control_mtime()
            if mtime is not None and mtime != seen_mtime:
                seen_mtime = mtime
                try:
                    with open(CONTROL_FILE) as cf:
                        data = json.load(cf)
                    if data.get("buzz"):
                        buzz()
                    if data.get("profile") in profiles:
                        apply(**profiles[data["profile"]])
                    apply(**{k: v for k, v in data.items() if k in SETTINGS})
                except (OSError, ValueError) as e:
                    print(f"? {CONTROL_FILE}: {e}")
                except pylink.errors.JLinkException:
                    pass  # applied by arm() after reconnecting
            time.sleep(0.05)
    print(f"log: {log_path}")


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--elf", default=DEFAULT_ELF)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("monitor")
    c = sub.add_parser("calibrate")
    c.add_argument("--current", type=float, default=0.2)
    c.add_argument("--seconds", type=float, default=1.0, help="give up if the shaft has not moved by then")
    r = sub.add_parser("run")
    r.add_argument("--mount", choices=sorted(MOUNTS), default="string",
                   help="settings file in host/mounts/: string = wound on the shaft, "
                        "hinge = shaft fixed to the door at the hinge")
    r.add_argument("--level", type=float, default=0.0, help="start at take-up only, raise it live")
    # Unset flags take the mounting's defaults from MOUNTS
    r.add_argument("--i-take", dest="take", type=float, help="[A] take-up / idle current")
    r.add_argument("--i-max", dest="max", type=float, help="[A] at level 1")
    r.add_argument("--free", type=int, help="counts of opening with take-up only (24 per turn)")
    r.add_argument("--ramp", type=int, help="counts over which resistance builds up to the level")
    r.add_argument("--damp", type=float, help="[A per count/s] extra resistance with opening speed")
    r.add_argument("--full", type=int, help="counts that count as fully opened (less = gave up)")
    r.add_argument("--open-dir", type=int, choices=(-1, 1),
                   help="sign of the count change when the door opens (see monitor); default from the mount file")
    r.add_argument("--wind-sign", type=int, choices=(-1, 1),
                   help="sign of the current that closes the door; on this motor +current lowers pos, "
                        "so it equals --open-dir; default from the mount file")
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
