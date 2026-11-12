#!/usr/bin/env python3
"""analyze.py - tables and graphs for Case Study 4 from results/raw.log."""
import csv
import statistics as st
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = "results"
PHYS = 2
recs = []
for line in open(f"{OUT}/raw.log"):
    d = {}
    for tok in line.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    recs.append(d)

out = []
emit = out.append
g = next(d for d in recs if d.get("exp") == "graph")
n, m = int(g["n"]), int(g["m"])
emit("## Graph")
emit("")
emit(f"R-MAT scale 20 (a,b,c,d = 0.57,0.19,0.19,0.05), seed 2026: n = {n:,} vertices, m = {m:,} edges "
     f"(average degree {m / n:.2f}), max in-degree {int(g['maxin']):,}, max out-degree {int(g['maxout']):,}, "
     f"dangling (out-degree 0) {int(g['dangling']):,} ({100 * int(g['dangling']) / n:.1f}%), "
     f"isolated {int(g['isolated']):,}.")

def med(rs, k): return st.median(float(r[k]) for r in rs)
def mn(rs, k): return min(float(r[k]) for r in rs)

# ---------------- verification ----------------
emit(""); emit("## Verification against serial ranks (tolerance: L1 distance < 1e-9)"); emit("")
emit("| version | p | schedule | iterations | L1 distance to serial | result |")
emit("|---|---|---|---|---|---|")
for d in recs:
    if d.get("exp") == "verify":
        emit(f"| {d['version']} | {d['p']} | {d['sched']} | {d['iters']} | {float(d['l1']):.3e} | "
             f"{'PASS' if d['pass'] == '1' else 'FAIL'} |")

# ---------------- strong scaling ----------------
strong = defaultdict(list)
for d in recs:
    if d.get("exp") == "strong":
        strong[(d["version"], d["sched"], int(d["p"]))].append(d)
ser = strong[("serial", "none", 1)]
T1, T1m = med(ser, "time"), mn(ser, "time")
iters = int(ser[0]["iters"])
emit(""); emit(f"## Strong scaling (median of {len(ser)} runs; serial {T1:.3f} s for {iters} iterations)"); emit("")
emit("| version | schedule | p | median (s) | min (s) | S median | E median | S min | M edges/s | imbalance R | edges max/mean |")
emit("|---|---|---|---|---|---|---|---|---|---|---|")
emit(f"| serial | - | 1 | {T1:.3f} | {T1m:.3f} | 1.00 | 1.00 | 1.00 | {m * iters / T1 / 1e6:.0f} | 1.00 | 1.00 |")
S = {}
for key in sorted(k for k in strong if k[0] != "serial"):
    v, s, p = key
    rs = strong[key]
    tm = med(rs, "time")
    S[key] = T1 / tm
    emit(f"| {v} | {s} | {p} | {tm:.3f} | {mn(rs, 'time'):.3f} | {T1 / tm:.2f} | {T1 / tm / p:.2f} | "
         f"{T1m / mn(rs, 'time'):.2f} | {m * int(rs[0]['iters']) / tm / 1e6:.0f} | {med(rs, 'imb'):.3f} | {med(rs, 'eshare'):.3f} |")

# ---------------- schedule sweep ----------------
sw = defaultdict(list)
for d in recs:
    if d.get("exp") == "sweep":
        sw[(int(d["p"]), d["sched"])].append(d)
order = ["static", "static,64", "static,1024", "dynamic,1", "dynamic,16", "dynamic,64", "dynamic,256",
         "dynamic,1024", "dynamic,4096", "dynamic,16384", "guided", "guided,64", "guided,256"]
emit(""); emit("## Schedule and chunk size (pull, median)"); emit("")
emit("| schedule | p=2 time (s) | p=2 R | p=2 edges max/mean | p=4 time (s) | p=4 R | p=4 edges max/mean |")
emit("|---|---|---|---|---|---|---|")
for s in order:
    a, b = sw.get((2, s)), sw.get((4, s))
    if not (a and b):
        continue
    emit(f"| {s} | {med(a, 'time'):.3f} | {med(a, 'imb'):.3f} | {med(a, 'eshare'):.3f} | "
         f"{med(b, 'time'):.3f} | {med(b, 'imb'):.3f} | {med(b, 'eshare'):.3f} |")

# ---------------- atomic cost ----------------
at = defaultdict(list)
for d in recs:
    if d.get("exp") == "atomic":
        at[d["version"]].append(d)
if at:
    pull1 = strong[("pull", "static", 1)]
    tpa, tpn, tpl = med(at["push"], "time"), med(at["pushna"], "time"), med(pull1, "time")
    it_ = int(at["push"][0]["iters"])
    emit(""); emit("## Cost of atomics (p = 1, static, median)"); emit("")
    emit("| version | time (s) | ns per edge per iteration |")
    emit("|---|---|---|")
    for name, t in (("pull (no atomics, random reads)", tpl), ("push without atomic (random writes)", tpn),
                    ("push with omp atomic (random atomic writes)", tpa)):
        emit(f"| {name} | {t:.3f} | {t / (m * it_) * 1e9:.2f} |")
    emit("")
    emit(f"Atomic overhead at p=1: {(tpa - tpn) / (m * it_) * 1e9:.2f} ns per edge "
         f"({100 * (tpa - tpn) / tpn:.0f}% on top of the plain scatter); push/pull ratio {tpa / tpl:.2f}.")

open(f"{OUT}/tables.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))

# ---------------- figures ----------------
deg, cin, cout = [], [], []
for row in csv.DictReader(open(f"{OUT}/degrees.csv")):
    d = int(row["degree"])
    if d > 0:
        deg.append(d); cin.append(int(row["in_count"])); cout.append(int(row["out_count"]))
fig, ax = plt.subplots(figsize=(5.5, 4))
ax.loglog([d for d, c in zip(deg, cin) if c], [c for c in cin if c], ".", ms=3, label="in-degree")
ax.loglog([d for d, c in zip(deg, cout) if c], [c for c in cout if c], ".", ms=3, alpha=0.6, label="out-degree")
ax.set_xlabel("degree"); ax.set_ylabel("number of vertices")
ax.set_title(f"CS4 degree distribution, R-MAT scale 20 (m = {m / 1e6:.1f} M)")
ax.grid(alpha=0.3, which="both"); ax.legend()
fig.tight_layout(); fig.savefig(f"{OUT}/degrees.png", dpi=200)

fig, axes = plt.subplots(1, 2, figsize=(11, 4.2))
for v, ax in zip(("pull", "push"), axes):
    for s, col in (("static", "tab:red"), ("dynamic,256", "tab:blue"), ("guided", "tab:green")):
        ys = [S.get((v, s, p)) for p in (1, 2, 4)]
        if None not in ys:
            ax.plot([1, 2, 4], ys, "o-", color=col, label=s)
    ax.plot([1, 2, 4], [1, 2, 4], ":", color="grey", label="ideal")
    ax.axvspan(PHYS, 4.3, color="orange", alpha=0.12, label="hyper-threading")
    ax.set_title(f"{v} (median of {len(ser)} runs)"); ax.set_xticks([1, 2, 4])
    ax.set_xlabel("threads p"); ax.set_ylabel("speedup vs serial pull"); ax.grid(alpha=0.3)
axes[0].legend(fontsize=8)
fig.tight_layout(); fig.savefig(f"{OUT}/speedup.png", dpi=200); fig.savefig(f"{OUT}/speedup.pdf")

fig, ax = plt.subplots(figsize=(6.5, 4))
chunks = [1, 16, 64, 256, 1024, 4096, 16384]
for p, col in ((2, "tab:blue"), (4, "tab:purple")):
    ys = [med(sw[(p, f"dynamic,{c}")], "time") if sw.get((p, f"dynamic,{c}")) else None for c in chunks]
    ax.semilogx(chunks, ys, "o-", color=col, label=f"dynamic,c  p={p}")
    if sw.get((p, "static")):
        ax.axhline(med(sw[(p, "static")], "time"), ls="--", color=col, alpha=0.6, label=f"static  p={p}")
    if sw.get((p, "guided")):
        ax.axhline(med(sw[(p, "guided")], "time"), ls=":", color=col, alpha=0.8, label=f"guided  p={p}")
ax.set_xlabel("chunk size c (vertices)"); ax.set_ylabel("time (s), median")
ax.set_title("CS4 pull: schedule and chunk size"); ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=7.5)
fig.tight_layout(); fig.savefig(f"{OUT}/schedules.png", dpi=200)
print(f"\nwrote {OUT}/tables.md, degrees.png, speedup.png, schedules.png")
