# Plot the EMA experiment: python tools/plot_ema.py docs/data/ema_experiment.csv docs/img/ema_comparison.png (needs matplotlib)
import csv, sys
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

src, out = sys.argv[1], sys.argv[2]
rows = list(csv.DictReader(open(src)))
t = [float(r["uptime_ms"]) / 1000 for r in rows]
col = lambda k: [float(r[k]) for r in rows]

series = {
    "T": ("Temperature, °C", ["raw_t", "t_a005", "t_a02", "t_a05"]),
    "H": ("Humidity, %RH", ["raw_h", "h_a005", "h_a02", "h_a05"]),
}
labels = ["raw", "α = 0.05", "α = 0.2 (chosen)", "α = 0.5"]
colors = ["#8a8a8a", "#1f77b4", "#d62728", "#2ca02c"]
styles = ["-", "--", "-", "-."]
widths = [1.2, 1.6, 2.4, 1.6]

fig, axes = plt.subplots(2, 1, figsize=(9, 7), sharex=True)
for ax, (key, (ylabel, cols)) in zip(axes, series.items()):
    for c, lab, color, ls, lw in zip(cols, labels, colors, styles, widths):
        ax.plot(t, col(c), ls, color=color, lw=lw, label=lab, marker="o" if c.startswith("raw") else None, ms=3)
    ax.axvspan(95, 140, color="#f2d27a", alpha=0.25, lw=0)
    ax.set_ylabel(ylabel)
    ax.grid(True, alpha=0.3)
axes[0].legend(loc="upper right", fontsize=9)
axes[0].text(97, axes[0].get_ylim()[0] + 0.1, "breath on the sensor", fontsize=9, va="bottom")
axes[1].set_xlabel("Time since boot, s (one sample every 5 s)")
fig.suptitle("EMA filter comparison on real BME280 data")
fig.tight_layout()
fig.savefig(out, dpi=140)

# numbers for the write-up
def idx(sec):
    return min(range(len(t)), key=lambda i: abs(t[i] - sec))

for key, (ylabel, cols) in series.items():
    print(key)
    for c, lab in zip(cols, labels):
        v = col(c)
        base = v[:18]
        print(f"  {lab:22s} baseline {min(base):.2f}..{max(base):.2f}  peak {max(v):.2f}  end {v[-1]:.2f}")
