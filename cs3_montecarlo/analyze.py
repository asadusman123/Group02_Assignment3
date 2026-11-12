#!/usr/bin/env python3
"""analyze.py - turns results/*.csv from bench.sh into tables and graphs.

Outputs (in results/):
  tables.md        speedup/efficiency, fixed-time accuracy, RNG-mode tables
  speedup.png/pdf  speedup + efficiency vs threads
"""
import csv, math, statistics, sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = "results"
PHYS_CORES = int(sys.argv[1]) if len(sys.argv) > 1 else 2   # i5-7300U: 2 physical cores
BS_PRICE = 10.450583572185565

def read(name):
    with open(f"{OUT}/{name}") as f:
        return list(csv.DictReader(f))

# ---------- strong scaling ----------
rows = read("strong.csv")
times = defaultdict(list)
for r in rows:
    times[(r["version"], int(r["p"]))].append(float(r["time"]))
nruns = len(times[("serial", 1)])

T1 = statistics.median(times[("serial", 1)])
ps = sorted(p for v, p in times if v == "parallel")
lines = ["## Strong scaling (M fixed)", "",
         f"Median of {nruns} interleaved runs. Speedup is relative to the serial baseline "
         f"(T1 = {T1:.3f} s). p > {PHYS_CORES} uses hyper-threading.", "",
         "| version | p | median time (s) | min | max | speedup S(p) | efficiency E(p) |",
         "|---|---|---|---|---|---|---|"]
t = times[("serial", 1)]
lines.append(f"| serial | 1 | {T1:.3f} | {min(t):.3f} | {max(t):.3f} | 1.00 | 1.00 |")
S, E = {}, {}
for p in ps:
    t = times[("parallel", p)]
    tp = statistics.median(t)
    S[p] = T1 / tp
    E[p] = S[p] / p
    lines.append(f"| parallel | {p} | {tp:.3f} | {min(t):.3f} | {max(t):.3f} | {S[p]:.2f} | {E[p]:.2f} |")

# Karp-Flatt experimentally determined serial fraction
lines += ["", "Experimentally determined serial fraction (Karp-Flatt), "
          "f = (1/S - 1/p) / (1 - 1/p):", ""]
for p in ps:
    if p > 1:
        f = (1 / S[p] - 1 / p) / (1 - 1 / p)
        lines.append(f"- p = {p}: f = {f:.3f}")

# ---------- fixed-time accuracy ----------
try:
    ft = read("fixed_time.csv")
    lines += ["", "## Fixed-time runs (accuracy per second)", "",
              "| p | samples M | time (s) | price | std. error | deviation from BS (in SE) |",
              "|---|---|---|---|---|---|"]
    for r in ft:
        se, pr = float(r["se"]), float(r["price"])
        lines.append(f"| {r['p']} | {int(r['M']):,} | {float(r['time']):.3f} | {pr:.6f} | "
                     f"{se:.6f} | {abs(pr - BS_PRICE) / se:.2f} |")
except FileNotFoundError:
    pass

# ---------- RNG strategies ----------
try:
    rm = read("rng_modes.csv")
    lines += ["", "## RNG strategies", "",
              "| mode | p | M | time (s) | M samples/s | price | reported SE | true SE* |",
              "|---|---|---|---|---|---|---|---|"]
    for r in rm:
        p, M, se = int(r["p"]), int(r["M"]), float(r["se"])
        true_se = se * math.sqrt(p) if r["mode"] == "sameseed" else se
        lines.append(f"| {r['mode']} | {p} | {M:,} | {float(r['time']):.3f} | "
                     f"{M / float(r['time']) / 1e6:.2f} | {float(r['price']):.6f} | "
                     f"{se:.6f} | {true_se:.6f} |")
    lines += ["", "*sameseed: all p threads replay one stream, so only M/p samples are "
              "independent and the true SE is sqrt(p) times the reported one."]
except FileNotFoundError:
    pass

open(f"{OUT}/tables.md", "w").write("\n".join(lines) + "\n")
print("\n".join(lines))

# ---------- graph ----------
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4))
xs = ps
ax1.plot(xs, [S[p] for p in xs], "o-", label="measured")
ax1.plot(xs, xs, "--", color="grey", label="ideal S(p) = p")
ax1.axvspan(PHYS_CORES, max(xs) + 0.3, color="orange", alpha=0.12,
            label="hyper-threading (p > physical cores)")
ax1.set_xlabel("threads p")
ax1.set_ylabel("speedup S(p) = T_serial / T_p")
ax1.set_xticks(xs)
ax1.set_title(f"CS3 Monte Carlo, M = {int(rows[0]['M']):.0e}")
ax1.legend(fontsize=8)
ax1.grid(alpha=0.3)

ax2.plot(xs, [E[p] for p in xs], "s-", color="tab:green")
ax2.axhline(1.0, ls="--", color="grey")
ax2.axvspan(PHYS_CORES, max(xs) + 0.3, color="orange", alpha=0.12)
ax2.set_xlabel("threads p")
ax2.set_ylabel("efficiency E(p) = S(p) / p")
ax2.set_xticks(xs)
ax2.set_ylim(0, 1.1)
ax2.set_title(f"each point = median of {nruns} runs")
ax2.grid(alpha=0.3)

fig.tight_layout()
fig.savefig(f"{OUT}/speedup.png", dpi=200)
fig.savefig(f"{OUT}/speedup.pdf")
print(f"\nwrote {OUT}/tables.md, {OUT}/speedup.png, {OUT}/speedup.pdf")
