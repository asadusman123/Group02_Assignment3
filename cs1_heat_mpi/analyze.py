#!/usr/bin/env python3
"""analyze.py - tables and graphs for Case Study 1 from results/raw.log."""
import math
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

def label(d):
    if d["version"] == "serial":
        return ("serial", "-", 1)
    return (d["dims"], d["comm"], int(d["p"]))

# ---------------- verification ----------------
ver = [d for d in recs if d.get("exp") == "verify"]
if ver:
    ok = sum(d["result"] == "IDENTICAL" for d in ver)
    emit(f"## Verification: final grid compared byte-for-byte with serial (N={ver[0]['N']}, T={ver[0]['T']})")
    emit("")
    emit(f"{ok} of {len(ver)} runs bit-identical to the serial output.")
    emit("")
    emit("| decomposition | mode | p=1 | p=2 | p=4 |")
    emit("|---|---|---|---|---|")
    tab = defaultdict(dict)
    for d in ver:
        tab[(d["decomp"], d["mode"])][int(d["p"])] = d["result"]
    for (dc, m), row in sorted(tab.items()):
        emit(f"| {dc} | {m} | {row.get(1, '')} | {row.get(2, '')} | {row.get(4, '')} |")

# ---------------- strong scaling ----------------
strong = defaultdict(lambda: defaultdict(list))
for d in recs:
    if d.get("exp") == "strong":
        strong[int(d["N"])][label(d)].append(d)

def med(rs, key):
    return st.median(float(r[key]) for r in rs)

def mn(rs, key):
    return min(float(r[key]) for r in rs)

speed = {}
for N in sorted(strong):
    runs = strong[N]
    ser = runs[("serial", "-", 1)]
    T1, T1m = med(ser, "time"), mn(ser, "time")
    nr = len(ser)
    emit("")
    emit(f"## Strong scaling, N = {N}, T = {ser[0]['T']}, K = 50 (median of {nr} runs; serial {T1:.3f} s)")
    emit("")
    emit("| dims | halo mode | p | median (s) | min (s) | S median | E median | S min | E min | compute (s) | halo comm (s) | allreduce (s) | halo values/rank/step |")
    emit("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    emit(f"| serial | - | 1 | {T1:.3f} | {T1m:.3f} | 1.00 | 1.00 | 1.00 | 1.00 | {T1:.3f} | 0 | 0 | 0 |")
    for key in sorted(k for k in runs if k[0] != "serial"):
        dims, mode, p = key
        rs = runs[key]
        tm, tmin = med(rs, "time"), mn(rs, "time")
        speed[(N, dims, mode, p)] = (T1 / tm, T1m / tmin)
        emit(f"| {dims} | {mode} | {p} | {tm:.3f} | {tmin:.3f} | {T1 / tm:.2f} | {T1 / tm / p:.2f} | "
             f"{T1m / tmin:.2f} | {T1m / tmin / p:.2f} | {med(rs, 'comp'):.3f} | {med(rs, 'comm_t'):.3f} | "
             f"{med(rs, 'reduce'):.4f} | {int(rs[0]['halo']):,} |")

# ---------------- surface to volume ----------------
emit("")
emit("## Surface-to-volume per rank per step (formula vs. what the code exchanged)")
emit("")
emit("| N | p | decomposition | cells/rank N^2/p | halo values/rank (formula) | halo (measured, max rank) | halo / cells |")
emit("|---|---|---|---|---|---|---|")
for N in sorted(strong):
    for p in (2, 4):
        for dims in sorted({k[0] for k in strong[N] if k[2] == p}):
            rs = strong[N][(dims, "nonblocking", p)]
            r_, c_ = map(int, dims.split("x"))
            if c_ == 1:
                formula = 2 * N          # interior row block; edge blocks have N
                desc = "1-D rows"
            else:
                formula = 4 * N // int(round(math.sqrt(p)))
                desc = "2-D blocks"
            cells = N * N // p
            emit(f"| {N} | {p} | {dims} ({desc}) | {cells:,} | {formula:,} | {int(rs[0]['halo']):,} | {formula / cells:.4f} |")
emit("")
emit("Formula: 1-D interior block receives 2 rows of N values; 2-D interior block receives "
     "4 edges of N/sqrt(p). Edge blocks receive fewer (no neighbour beyond the boundary), "
     "which is why the measured maximum can be below the interior-block formula at small p.")

# ---------------- overlap ----------------
emit("")
emit("## Blocking vs non-blocking (overlap), median times")
emit("")
emit("| N | dims | p | blocking total (s) | blocking halo comm (s) | nonblocking total (s) | nonblocking wait (s) | nbtest total (s) | nbtest comm (s) | time saved by nonblocking |")
emit("|---|---|---|---|---|---|---|---|---|---|")
for N in sorted(strong):
    for key in sorted({(k[0], k[2]) for k in strong[N] if k[0] != "serial"}, key=lambda x: (x[1], x[0])):
        dims, p = key
        b = strong[N].get((dims, "blocking", p)); nb = strong[N].get((dims, "nonblocking", p)); nt = strong[N].get((dims, "nbtest", p))
        if not (b and nb and nt):
            continue
        tb, tn = med(b, "time"), med(nb, "time")
        emit(f"| {N} | {dims} | {p} | {tb:.3f} | {med(b, 'comm_t'):.3f} | {tn:.3f} | {med(nb, 'comm_t'):.3f} | "
             f"{med(nt, 'time'):.3f} | {med(nt, 'comm_t'):.3f} | {100 * (tb - tn) / tb:+.1f}% |")

# ---------------- model T_p = T_comp/p + T_comm ----------------
emit("")
emit("## Model T_p ~ T_serial/p + T_comm  (2-D, non-blocking; T_comm = measured halo + allreduce)")
emit("")
emit("| N | p | measured T_p (s) | T_serial/p (s) | T_comm (s) | model (s) | E median | E < 70%? |")
emit("|---|---|---|---|---|---|---|---|")
for N in sorted(strong):
    T1 = med(strong[N][("serial", "-", 1)], "time")
    for key in sorted(k for k in strong[N] if k[1] == "nonblocking" and (k[0] != "4x1")):
        dims, mode, p = key
        rs = strong[N][key]
        tp = med(rs, "time"); tc = med(rs, "comm_t") + med(rs, "reduce")
        E = T1 / tp / p
        emit(f"| {N} | {p} | {tp:.3f} | {T1 / p:.3f} | {tc:.3f} | {T1 / p + tc:.3f} | {E:.2f} | {'yes' if E < 0.7 else 'no'} |")

# ---------------- K cost ----------------
kc = defaultdict(list)
for d in recs:
    if d.get("exp") == "kcost":
        kc[(int(d["N"]), int(d["K"]), d["version"], int(d["p"]))].append(d)
if kc:
    emit("")
    emit("## Convergence-check cost: K = 1 vs K = 50 (median)")
    emit("")
    emit("| N | version | p | time K=1 (s) | time K=50 (s) | K=1 / K=50 | compute K=1 | compute K=50 | allreduce K=1 | allreduce K=50 |")
    emit("|---|---|---|---|---|---|---|---|---|---|")
    for N in sorted({k[0] for k in kc}):
        for v, p in (("serial", 1), ("parallel", 2), ("parallel", 4)):
            a = kc.get((N, 1, v, p)); b = kc.get((N, 50, v, p))
            if not (a and b):
                continue
            emit(f"| {N} | {v} | {p} | {med(a, 'time'):.3f} | {med(b, 'time'):.3f} | {med(a, 'time') / med(b, 'time'):.2f} | "
                 f"{med(a, 'comp'):.3f} | {med(b, 'comp'):.3f} | {med(a, 'reduce'):.4f} | {med(b, 'reduce'):.4f} |")

# ---------------- weak scaling ----------------
wk = defaultdict(list)
for d in recs:
    if d.get("exp") == "weak":
        wk[(d["version"], int(d["p"]) if d["version"] != "serial" else int(d["N"]))].append(d)
weak_rows = []
if wk:
    emit("")
    emit("## Weak scaling (2-D, non-blocking): N_p = N0*sqrt(p), cells per rank constant")
    emit("")
    emit("| p | N_p | cells/rank | T_p median (s) | T_p min (s) | serial on N_p (s) | weak efficiency T_1/T_p | speedup vs serial on same N |")
    emit("|---|---|---|---|---|---|---|---|")
    par = sorted((k[1], v) for k, v in wk.items() if k[0] == "parallel")
    T1w = med(par[0][1], "time")
    for p, rs in par:
        Np = int(rs[0]["N"])
        ser = wk.get(("serial", Np))
        ts = med(ser, "time") if ser else float("nan")
        tp = med(rs, "time")
        weak_rows.append((p, T1w / tp))
        emit(f"| {p} | {Np} | {Np * Np // p:,} | {tp:.3f} | {mn(rs, 'time'):.3f} | {ts:.3f} | {T1w / tp:.2f} | {ts / tp:.2f} |")

open(f"{OUT}/tables.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))

# ---------------- graphs ----------------
Ns = sorted(strong)
fig, axes = plt.subplots(1, len(Ns) + 1, figsize=(5 * (len(Ns) + 1), 4.2))
styles = {("blocking", "1d"): ("tab:red", "--"), ("nonblocking", "1d"): ("tab:blue", "--"), ("nbtest", "1d"): ("tab:green", "--"),
          ("blocking", "2d"): ("tab:red", "-"), ("nonblocking", "2d"): ("tab:blue", "-"), ("nbtest", "2d"): ("tab:green", "-")}
for ax, N in zip(axes, Ns):
    nr = len(strong[N][("serial", "-", 1)])
    for (mode, dec), (col, ls) in styles.items():
        ys = []
        for p in (1, 2, 4):
            dims = {1: "1x1", 2: "2x1", 4: "4x1" if dec == "1d" else "2x2"}[p]
            s = speed.get((N, dims, mode, p))
            ys.append(s[0] if s else None)
        if None in ys:
            continue
        ax.plot([1, 2, 4], ys, ls, marker="o", color=col, label=f"{dec} {mode}")
    ax.plot([1, 2, 4], [1, 2, 4], ":", color="grey", label="ideal")
    ax.axvspan(PHYS, 4.3, color="orange", alpha=0.12, label="p > physical cores")
    ax.set_title(f"strong scaling, N = {N} (median of {nr} runs)")
    ax.set_xlabel("MPI processes p"); ax.set_ylabel("speedup vs serial"); ax.set_xticks([1, 2, 4]); ax.grid(alpha=0.3)
axes[0].legend(fontsize=7)
ax = axes[-1]
if weak_rows:
    ps = [r[0] for r in weak_rows]
    ax.plot(ps, [r[1] for r in weak_rows], "o-", color="tab:purple", label="weak efficiency T_1 / T_p")
    ax.axhline(1, ls=":", color="grey")
    ax.axvspan(PHYS, 4.3, color="orange", alpha=0.12)
    ax.set_ylim(0, 1.2); ax.set_xticks(ps)
    ax.set_xlabel("MPI processes p"); ax.set_ylabel("efficiency")
    ax.set_title(f"weak scaling, {int(wk[('parallel', 1)][0]['N'])}^2 cells per rank (median of {len(wk[('parallel', 1)])} runs)")
    ax.grid(alpha=0.3); ax.legend(fontsize=7)
fig.tight_layout()
fig.savefig(f"{OUT}/scaling.png", dpi=200); fig.savefig(f"{OUT}/scaling.pdf")

# stacked phase bars for the big N
N = Ns[-1]
keys = [k for k in sorted(strong[N], key=lambda k: (k[2], k[0], k[1])) if k[0] != "serial"]
fig, ax = plt.subplots(figsize=(10, 4))
xs = range(len(keys))
comp = [med(strong[N][k], "comp") for k in keys]
comm = [med(strong[N][k], "comm_t") for k in keys]
red = [med(strong[N][k], "reduce") for k in keys]
ax.bar(xs, comp, label="compute", color="tab:blue")
ax.bar(xs, comm, bottom=comp, label="halo communication / wait", color="tab:orange")
ax.bar(xs, red, bottom=[a + b for a, b in zip(comp, comm)], label="allreduce", color="tab:red")
ax.set_xticks(list(xs)); ax.set_xticklabels([f"p={k[2]}\n{k[0]}\n{k[1]}" for k in keys], fontsize=7)
ax.set_ylabel("seconds (max over ranks, median of runs)")
ax.set_title(f"CS1 phase breakdown, N = {N}, T = {strong[N][keys[0]][0]['T']}")
ax.legend(fontsize=8); ax.grid(axis="y", alpha=0.3)
fig.tight_layout(); fig.savefig(f"{OUT}/phases.png", dpi=200)
print(f"\nwrote {OUT}/tables.md, scaling.png, phases.png")
