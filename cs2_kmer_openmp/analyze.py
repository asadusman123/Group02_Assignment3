#!/usr/bin/env python3
"""analyze.py - tables and graphs for Case Study 2 from results/raw.log."""
import statistics as st
import sys
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = "results"
PHYS = int(sys.argv[1]) if len(sys.argv) > 1 else 2
L = None

recs = []
for line in open(f"{OUT}/raw.log"):
    d = {}
    for tok in line.split():
        if "=" in tok:
            key, val = tok.split("=", 1)
            d[key] = val
    if "RESULT" in line and "membench" in line:
        d["version"] = "membench"
    recs.append(d)

def num(d, key):
    return float(d[key])

out = []
def emit(s=""):
    out.append(s)

# ---------------- correctness ----------------
emit("## Correctness (L = 1e8, every bin compared with the serial histogram)")
emit()
emit("| dist | k | version | p | total counted | expected L-k+1 | lost | wrong bins | result |")
emit("|---|---|---|---|---|---|---|---|---|")
for d in recs:
    if d.get("exp") != "correct" or d.get("version") == "serial":
        continue
    k = int(d["k"]); Lv = int(d["L"]); L = Lv
    exp_total = Lv - k + 1
    tot = int(d["total"]); bad = int(d["badbins"])
    emit(f"| {d['dist']} | {k} | {d['version']} | {d['p']} | {tot:,} | {exp_total:,} | "
         f"{exp_total - tot:,} | {bad:,} | {'PASS' if bad == 0 else 'FAIL'} |")
emit()
emit("Version N cuts the sequence (not the k-mer start positions) into p pieces; it must lose "
     "exactly (p-1)(k-1) k-mers: 7/21 for k=8 and 11/33 for k=12 at p=2/4.")

# ---------------- strong scaling ----------------
times = defaultdict(list)
for d in recs:
    if d.get("exp") == "strong":
        times[(d["dist"], int(d["k"]), d["version"], int(d["p"]))].append(num(d, "time"))
nrounds = len(next(iter(times.values()))) if times else 0
S_med, S_min = {}, {}
for dist in ("uniform", "skewed"):
    for k in (8, 12):
        key0 = (dist, k, "serial", 1)
        if key0 not in times:
            continue
        T1 = st.median(times[key0]); T1m = min(times[key0])
        nk = (L or 100000000) - k + 1
        emit(); emit(f"## Strong scaling: {dist}, k = {k}  (median of {nrounds} runs; T_serial = {T1:.4f} s)")
        emit()
        emit("| version | p | median (s) | min (s) | max (s) | M k-mers/s (median) | S median | E median | S min | E min |")
        emit("|---|---|---|---|---|---|---|---|---|---|")
        t = times[key0]
        emit(f"| serial | 1 | {T1:.4f} | {min(t):.4f} | {max(t):.4f} | {nk / T1 / 1e6:.0f} | 1.00 | 1.00 | 1.00 | 1.00 |")
        for v in ("B", "C"):
            for p in (1, 2, 4):
                t = times.get((dist, k, v, p))
                if not t:
                    continue
                tm = st.median(t)
                S_med[(dist, k, v, p)] = T1 / tm
                S_min[(dist, k, v, p)] = T1m / min(t)
                emit(f"| {v} | {p} | {tm:.4f} | {min(t):.4f} | {max(t):.4f} | {nk / tm / 1e6:.0f} | "
                     f"{T1 / tm:.2f} | {T1 / tm / p:.2f} | {T1m / min(t):.2f} | {T1m / min(t) / p:.2f} |")

# ---------------- atomic vs private ----------------
emit(); emit("## Atomic (B) vs private histograms (C), uniform, median times")
emit()
emit("| k | p | B (s) | C (s) | B/C | C extra memory (p x 4^k x 4 B) |")
emit("|---|---|---|---|---|---|")
for k in (8, 12):
    for p in (1, 2, 4):
        b = times.get(("uniform", k, "B", p)); c = times.get(("uniform", k, "C", p))
        if b and c:
            extra = p * (4 ** k) * 4
            ext = f"{extra / 1024:.0f} KiB" if extra < 2**20 else f"{extra / 2**20:.0f} MiB"
            emit(f"| {k} | {p} | {st.median(b):.4f} | {st.median(c):.4f} | {st.median(b) / st.median(c):.2f} | {ext} |")

merges = defaultdict(list)
for d in recs:
    if d.get("exp") == "strong" and d.get("version") == "C":
        merges[(d["dist"], int(d["k"]), int(d["p"]))].append(num(d, "merge"))
emit(); emit("Version C merge time (median, uniform): " + ", ".join(
    f"k={k} p={p}: {st.median(merges[('uniform', k, p)]) * 1000:.1f} ms"
    for k in (8, 12) for p in (1, 2, 4) if merges.get(("uniform", k, p))))

# ---------------- false sharing ----------------
fs = defaultdict(list)
for d in recs:
    if d.get("exp") == "falseshare":
        fs[(d["version"], int(d["p"]))].append(num(d, "time"))
if fs:
    emit(); emit(f"## False sharing (k = 1, per-thread counters in one shared array; median of {nrounds} runs)")
    emit()
    emit("| p | unpadded 32 B/thread (s) | padded 128 B/thread (s) | unpadded / padded | unpadded speedup vs p=1 | padded speedup vs p=1 |")
    emit("|---|---|---|---|---|---|")
    u1 = st.median(fs[("fs", 1)]); p1 = st.median(fs[("fspad", 1)])
    for p in (1, 2, 4):
        if fs.get(("fs", p)) and fs.get(("fspad", p)):
            u = st.median(fs[("fs", p)]); pd = st.median(fs[("fspad", p)])
            emit(f"| {p} | {u:.4f} | {pd:.4f} | {u / pd:.2f} | {u1 / u:.2f} | {p1 / pd:.2f} |")

# ---------------- memory probe ----------------
mb = defaultdict(lambda: defaultdict(list))
for d in recs:
    if d.get("exp") == "membench":
        p = int(d["p"])
        for key in ("read_gbs", "rmw_256k_mps", "rmw_64m_mps"):
            mb[p][key].append(float(d[key]))
if mb:
    emit(); emit("## Memory probe (membench, best of 3 rounds)")
    emit()
    emit("| p | sequential read GB/s | random RMW, 256 KiB table (M/s) | random RMW, 64 MiB table (M/s) |")
    emit("|---|---|---|---|")
    for p in sorted(mb):
        emit(f"| {p} | {max(mb[p]['read_gbs']):.2f} | {max(mb[p]['rmw_256k_mps']):.0f} | {max(mb[p]['rmw_64m_mps']):.0f} |")

open(f"{OUT}/tables.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))

# ---------------- graph ----------------
fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=True)
ps = [1, 2, 4]
for ax, k in zip(axes, (8, 12)):
    for v, col in (("B", "tab:red"), ("C", "tab:blue")):
        for dist, ls in (("uniform", "-"), ("skewed", "--")):
            ys = [S_med.get((dist, k, v, p)) for p in ps]
            if None in ys:
                continue
            name = {"B": "B atomic", "C": "C private"}[v]
            ax.plot(ps, ys, ls, marker="o", color=col, label=f"{name}, {dist}")
    ax.plot(ps, ps, ":", color="grey", label="ideal")
    ax.axvspan(PHYS, 4.3, color="orange", alpha=0.12, label="hyper-threading")
    ax.set_title(f"k = {k}  ({4 ** k * 4 / 1024:.0f} KiB histogram)" if k == 8 else
                 f"k = {k}  ({4 ** k * 4 / 2**20:.0f} MiB histogram)")
    ax.set_xlabel("threads p")
    ax.set_xticks(ps)
    ax.grid(alpha=0.3)
axes[0].set_ylabel("speedup vs serial  (median of %d runs)" % nrounds)
axes[1].legend(fontsize=7.5, loc="upper left")
fig.suptitle(f"CS2 k-mer counting, L = {L:.0e}")
fig.tight_layout()
fig.savefig(f"{OUT}/speedup.png", dpi=200)
fig.savefig(f"{OUT}/speedup.pdf")
print(f"\nwrote {OUT}/tables.md, {OUT}/speedup.png")
