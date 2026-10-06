#!/usr/bin/env python3
"""Turn a raw `pio device monitor -f log2file` capture into a redacted evidence file.

Usage: python tools/summarize_long_run.py <raw.log> > docs/logs/phase7-long-run.txt

The raw log stays local (it is git-ignored). The output keeps the boot sequence, the first and last
snapshots, every warning/error with its context, and counters computed over the whole file.
SSID, IP addresses, MAC addresses and the Windows user name are replaced with placeholders.
"""
import re
import sys
from collections import Counter

if len(sys.argv) != 2:
    sys.exit(__doc__)

lines = open(sys.argv[1], encoding="utf-8", errors="replace").read().splitlines()
raw_count = len(lines)
# The capture has an empty line after every line (\r\r\n), so only non-empty lines are kept.
lines = [l.rstrip("\r") for l in lines if l.strip()]


def redact(s: str) -> str:
    s = re.sub(r'SSID "[^"]*"', 'SSID "<ssid>"', s)
    s = re.sub(r"connected with \S+,", "connected with <ssid>,", s)
    s = re.sub(r"\b(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}\b", "<mac>", s)
    s = re.sub(r"\b\d{1,3}(?:\.\d{1,3}){3}\b", "<ip>", s)
    s = re.sub(r"C:\\Users\\[^\\]+", r"C:\\Users\\<user>", s)
    return s


DATA = re.compile(r"^\[(\d\d):(\d\d):(\d\d)\] T:")
DIAG = re.compile(r"uptime (\d+) s, heap free (\d+) B \(lowest (\d+) B\), CPU load ([\d.]+) %")
STACK = re.compile(r"diag:\s+(\S+)\s+\w\s+prio\s+\d+\s+stack free\s+(\d+) B")
TIMING = re.compile(r"T1: timing over \d+ cycles, us min/avg/max: I2C start (\d+)/(\d+)/(\d+), "
                    r"I2C read (\d+)/(\d+)/(\d+), cycle (\d+)/(\d+)/(\d+)")

data_idx = [i for i, l in enumerate(lines) if DATA.match(l)]
secs = []
for i in data_idx:
    m = DATA.match(lines[i])
    secs.append(int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]))
gaps = Counter((b - a) % 86400 for a, b in zip(secs, secs[1:]))
slow = [(data_idx[k + 1] + 1, lines[data_idx[k + 1]][:10]) for k, (a, b) in enumerate(zip(secs, secs[1:])) if (b - a) % 86400 != 5]

diag = [(i, DIAG.search(l)) for i, l in enumerate(lines) if DIAG.search(l)]
diag_v = [(int(m[1]), int(m[2]), int(m[3]), float(m[4])) for _, m in diag]
stack_min = {}
for l in lines:
    m = STACK.search(l)
    if m:
        stack_min[m[1]] = min(stack_min.get(m[1], 1 << 30), int(m[2]))
timings = [tuple(map(int, m.groups())) for m in (TIMING.search(l) for l in lines) if m]

bad = [i for i, l in enumerate(lines) if re.match(r"^[WE] \(", l)]
bad_noise = re.compile(r"spi_flash: Detected size|i2c\.master: Please check pull-up")
flagged = [l for l in lines if DATA.match(l) and re.search(r"!SENS|!TIME", l)]
err_vals = Counter(re.search(r"ERR:(\d+)", l)[1] for l in lines if DATA.match(l))


def block(title, rows):
    print(f"\n=== {title} ===")
    for r in rows:
        print(redact(r))


last_uptime = diag_v[-1][0] if diag_v else 0
print("Phase 7 long-run log (ESP32-S3-DevKitM-1, ESP-IDF 5.5.3)")
print("Captured with: pio device monitor -f log2file (115200 8-N-1), board powered over USB, nobody touched it.")
print("Produced by tools/summarize_long_run.py from the raw capture; private data (SSID, IP, MAC) is replaced with placeholders.")
print("The raw file is not committed: %d non-empty lines (%d with the empty lines the capture inserts), about %.1f MB."
      % (len(lines), raw_count, sum(len(l) + 2 for l in lines) / 1e6))

print("\n=== Summary computed over the whole file ===")
print(f"Last diagnostics snapshot: uptime {last_uptime} s = {last_uptime // 3600} h {last_uptime % 3600 // 60} min")
print(f"First / last log line time: {lines[data_idx[0]][1:9]} / {lines[data_idx[-1]][1:9]} (DS3231)")
print(f"Log lines: {len(data_idx)}; expected for that uptime at one per 5 s: about {last_uptime // 5}")
print(f"Spacing between consecutive lines (RTC seconds): {dict(sorted(gaps.items()))}")
if slow:
    print("Lines that came 6 s after the previous one (RTC has whole-second resolution): " +
          ", ".join(f"non-empty line {n} ({t})" for n, t in slow))
print(f"ERR values seen: {dict(err_vals)}; lines with !SENS or !TIME: {len(flagged)}")
print(f"Boot banners (rst:0x...): {sum(1 for l in lines if 'rst:0x' in l)}; "
      f"reset-reason lines: {sum(1 for l in lines if 'reset: reset reason' in l)}; "
      f"watchdog, panic, backtrace or abort messages: "
      f"{sum(1 for l in lines if re.search(r'task_wdt|Guru Meditation|Backtrace|abort\(\)', l))}")
print(f"Warnings and errors, excluding the two boot-time notices: "
      f"{sum(1 for i in bad if not bad_noise.search(lines[i]))} lines (listed below)")
if diag_v:
    print(f"Diagnostics snapshots: {len(diag_v)} (one per minute)")
    print(f"Free heap: min {min(v[1] for v in diag_v)} B, max {max(v[1] for v in diag_v)} B; "
          f"first {diag_v[0][1]} B (60 s), last {diag_v[-1][1]} B")
    print(f"Lowest-ever free heap: {diag_v[0][2]} B at 60 s, {diag_v[-1][2]} B at the end "
          f"(difference {diag_v[0][2] - diag_v[-1][2]} B)")
    print(f"CPU load: min {min(v[3] for v in diag_v)} %, max {max(v[3] for v in diag_v)} %, "
          f"mean {sum(v[3] for v in diag_v) / len(diag_v):.2f} %")
if stack_min:
    print("Smallest free stack seen per task (bytes): " + ", ".join(f"{k} {v}" for k, v in sorted(stack_min.items())))
if timings:
    print(f"T1 timing reports: {len(timings)}; I2C start min/max {min(t[0] for t in timings)}/{max(t[2] for t in timings)} us, "
          f"I2C read {min(t[3] for t in timings)}/{max(t[5] for t in timings)} us, "
          f"cycle {min(t[6] for t in timings)}/{max(t[8] for t in timings)} us")

# MQTT outages
outages = []
for i, l in enumerate(lines):
    if "mqtt: disconnected from the broker" in l:
        t0 = int(re.match(r"\w \((\d+)\)", l)[1])
        for l2 in lines[i + 1:]:
            if "mqtt: connected to the broker" in l2:
                outages.append((t0, int(re.match(r"\w \((\d+)\)", l2)[1])))
                break
        else:
            outages.append((t0, None))
print(f"MQTT disconnects: {len(outages)}" + "".join(
    f"; at {a / 1000:.0f} s uptime" + (f", reconnected after {(b - a) / 1000:.1f} s" if b else ", not reconnected") for a, b in outages))

# boot sequence up to the first log line
first = data_idx[0]
start = next(i for i, l in enumerate(lines) if "Calling app_main" in l)
block("Boot sequence from app_main (bootloader and ROM lines omitted)", lines[start:first + 1])

# every warning / error with context
shown = set()
events = [i for i in bad if not bad_noise.search(lines[i])]
if events:
    print("\n=== Warnings and errors with context ===")
    groups = []
    for i in events:
        if groups and i - groups[-1][-1] <= 40:
            groups[-1].append(i)
        else:
            groups.append([i])
    for g in groups:
        lo, hi = max(g[0] - 2, 0), min(g[-1] + 12, len(lines))
        nxt = next((k for k in range(g[-1], len(lines)) if "mqtt: connected to the broker" in lines[k]), None)
        if nxt is not None and nxt - g[-1] < 200:
            hi = nxt + 2
        print("---")
        for j in range(lo, hi):
            if j not in shown and not (lines[j].startswith("I (") and "diag:" in lines[j]):
                print(redact(lines[j]))
                shown.add(j)

# first and last diagnostics snapshot (full)
for label, (i, _) in (("First diagnostics snapshot", diag[0]), ("Last diagnostics snapshot", diag[-1])) if diag else []:
    end = next((k for k in range(i + 1, len(lines)) if DIAG.search(lines[k])), len(lines))
    block(label, [l for l in lines[i:end] if "diag:" in l])

block("Last 5 log lines of the capture", lines[-5:])
