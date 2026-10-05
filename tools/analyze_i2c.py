# Decode and summarise the logic analyzer exports in docs/img/logic-analyzer (boot.csv, measurement.csv, nack-recovery.csv).
# Usage: python tools/analyze_i2c.py docs/img/logic-analyzer
# The CSV files are the I2C analyzer export of Saleae Logic 2 (start, address, data, stop rows).
import csv, sys, collections

base = sys.argv[1]


def load(name):
    rows = []
    with open(f"{base}/{name}", newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            rows.append({
                "type": r["type"], "t": float(r["start_time"]), "dur": float(r["duration"]),
                "ack": r["ack"], "addr": r["address"], "read": r["read"], "data": r["data"],
            })
    return rows


def transactions(rows):
    """Group into START..STOP transactions."""
    out, cur = [], None
    for r in rows:
        if r["type"] == "start":
            cur = {"t0": r["t"], "addr": None, "rw": None, "ack_addr": None, "bytes": [], "t1": None}
        elif cur is None:
            continue
        elif r["type"] == "address":
            cur["addr"] = r["addr"]
            cur["rw"] = "R" if r["read"] == "true" else "W"
            cur["ack_addr"] = r["ack"] == "true"
            cur["dur_addr"] = r["dur"]
        elif r["type"] == "data":
            cur["bytes"].append((r["data"], r["ack"] == "true", r["dur"]))
        elif r["type"] == "stop":
            cur["t1"] = r["t"]
            out.append(cur)
            cur = None
    return out


def summary(name):
    rows = load(name)
    tr = transactions(rows)
    print(f"=== {name}: {len(rows)} decoded items, {len(tr)} transactions, "
          f"{rows[0]['t']:.6f}s .. {rows[-1]['t']:.6f}s")
    return rows, tr


# ---------------- boot ----------------
rows, tr = summary("boot.csv")
probes = [t for t in tr if not t["bytes"] and t["addr"] is not None]
acked = [t for t in probes if t["ack_addr"]]
nacked = [t for t in probes if not t["ack_addr"]]
print(f"address-only transactions (scan probes): {len(probes)}  ACK {len(acked)}  NACK {len(nacked)}")
print("ACKed probe addresses:", sorted({t['addr'] for t in acked}))
addrs = sorted({int(t['addr'], 16) for t in probes})
print(f"probed address range: 0x{addrs[0]:02X}..0x{addrs[-1]:02X}  distinct: {len(addrs)}")
durs = [t["dur_addr"] for t in probes]
print(f"probe address-byte duration: min {min(durs)*1e6:.1f} us  max {max(durs)*1e6:.1f} us")
scan_span = (probes[-1]["t1"] - probes[0]["t0"])
print(f"scan span: {scan_span*1e3:.2f} ms for {len(probes)} probes -> {scan_span/len(probes)*1e6:.0f} us per probe")
other = [t for t in tr if t["bytes"]]
print("transactions with data after the scan (first 12):")
for t in other[:12]:
    print(f"  t={t['t0']:.6f}s {t['addr']} {t['rw']} ack={t['ack_addr']} "
          f"bytes={[(b[0], 'A' if b[1] else 'N') for b in t['bytes']][:10]} len={len(t['bytes'])}")
fast = [t["dur_addr"] for t in other]
if fast:
    print(f"address-byte duration in data transactions: min {min(fast)*1e6:.1f} us max {max(fast)*1e6:.1f} us")
    print(f"ratio scan/data address byte: {min(durs)/min(fast):.2f} (expected about 4 for 100 vs 400 kHz)")

# ---------------- measurement ----------------
rows, tr = summary("measurement.csv")
prev = None
for t in tr:
    gap = (t["t0"] - prev) * 1e3 if prev is not None else 0.0
    bs = " ".join(f"{b[0]}{'' if b[1] else '(NACK)'}" for b in t["bytes"])
    print(f"  +{gap:8.3f} ms  t={t['t0']:.6f}s  {t['addr']} {t['rw']} {'ACK' if t['ack_addr'] else 'NACK'}  "
          f"len {len(t['bytes'])}  {bs[:90]}  dur {(t['t1']-t['t0'])*1e6:.0f} us")
    prev = t["t1"]
if tr:
    d = [t["dur_addr"] for t in tr]
    print(f"address-byte duration {min(d)*1e6:.3f} us -> if 9 clocks, SCL = {9/min(d)/1e3:.0f} kHz (upper bound of the reading)")
    print(f"first start -> last stop: {(tr[-1]['t1']-tr[0]['t0'])*1e3:.3f} ms")

# ---------------- nack recovery ----------------
rows, tr = summary("nack-recovery.csv")
prev = None
for t in tr:
    gap = (t["t0"] - prev) * 1e3 if prev is not None else 0.0
    bs = " ".join(f"{b[0]}{'' if b[1] else '(NACK)'}" for b in t["bytes"])
    print(f"  +{gap:9.3f} ms  t={t['t0']:.6f}s  {t['addr']} {t['rw']} {'ACK' if t['ack_addr'] else 'NACK'}  "
          f"len {len(t['bytes'])}  {bs[:60]}")
    prev = t["t1"]
nack = [t for t in tr if not t["ack_addr"]]
print("transactions whose address was NACKed:", len(nack), [f"{t['addr']}@{t['t0']:.4f}s" for t in nack])
