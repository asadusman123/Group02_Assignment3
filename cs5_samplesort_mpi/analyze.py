#!/usr/bin/env python3
"""analyze.py - tables and graphs for Case Study 5 from results/raw.log."""
import statistics as st
from collections import defaultdict

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OUT = "results"
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
def med(rs, k): return st.median(float(r[k]) for r in rs)
def mn(rs, k): return min(float(r[k]) for r in rs)

# ---------------- ping-pong fit ----------------
pp = defaultdict(list)
for d in recs:
    if d.get("exp") == "pingpong":
        pp[int(d["bytes"])].append(float(d["oneway"]))
sizes = sorted(pp)
tbest = {m: min(pp[m]) for m in sizes}
small = [m for m in sizes if m <= 64]
alpha = st.mean(tbest[m] for m in small)
# beta: least-squares slope over 64 KiB .. 16 MiB (bandwidth regime, still partly cache-resident)
big = [m for m in sizes if 65536 <= m <= 16 << 20]
mx, my = st.mean(big), st.mean(tbest[m] for m in big)
beta = sum((m - mx) * (tbest[m] - my) for m in big) / sum((m - mx) ** 2 for m in big)
# beta from the largest message (DRAM-to-DRAM copy)
mbig = sizes[-1]
beta_big = (tbest[mbig] - alpha) / mbig
emit("## Ping-pong: T(m) = alpha + beta*m  (best of 3 rounds, one-way time)")
emit("")
emit("| message | one-way time (us) | bandwidth (GB/s) | model alpha+beta*m (us) |")
emit("|---|---|---|---|")
for m in sizes:
    emit(f"| {m:,} B | {tbest[m] * 1e6:.2f} | {m / tbest[m] / 1e9:.2f} | {(alpha + beta * m) * 1e6:.2f} |")
emit("")
emit(f"alpha = {alpha * 1e6:.3f} us (mean of messages <= 64 B); beta = {beta * 1e9:.4f} ns/B "
     f"(= {1 / beta / 1e9:.2f} GB/s, fit 64 KiB-16 MiB); for a {mbig / 2**20:.0f} MiB message "
     f"beta_DRAM = {beta_big * 1e9:.4f} ns/B (= {1 / beta_big / 1e9:.2f} GB/s).")

# ---------------- oversampling ----------------
ovr = defaultdict(dict)
for d in recs:
    if d.get("exp") == "oversample":
        ovr[(int(d["p"]), d["dist"], int(d["tie"]))][int(d["ov"])] = d
emit(""); emit("## Load balance after redistribution: max/mean elements per rank vs oversampling ov (ov*p - 1 samples per rank)")
emit("")
ovs = sorted({o for v in ovr.values() for o in v})
emit("| p | distribution | splitter rule | " + " | ".join(f"ov={o}" for o in ovs) + " |")
emit("|---|---|---|" + "---|" * len(ovs))
for key in sorted(ovr):
    p, dist, tie = key
    emit(f"| {p} | {dist} | {'key+rank+pos' if tie else 'key only'} | " +
         " | ".join(f"{float(ovr[key][o]['ratio']):.3f}" if o in ovr[key] else "" for o in ovs) + " |")
emit("")
emit("Final elements per rank at ov = 1 and the largest ov:")
emit("")
emit("| p | distribution | splitter rule | ov | counts per rank |")
emit("|---|---|---|---|---|")
for key in sorted(ovr):
    for o in (ovs[0], ovs[-1]):
        if o in ovr[key]:
            emit(f"| {key[0]} | {key[1]} | {'key+rank+pos' if key[2] else 'key only'} | {o} | "
                 f"{ovr[key][o]['counts'].replace(',', ', ')} |")

# ---------------- timing ----------------
tm = defaultdict(list)
for d in recs:
    if d.get("exp") == "time":
        tm[(d["dist"], d["sort"], d["version"], int(d["p"]))].append(d)
nr = len(next(iter(tm.values()))) if tm else 0
phases = [("t_local", "local sort"), ("t_sample", "sampling"), ("t_part", "partition"),
          ("t_a2a", "alltoall(v)"), ("t_merge", "merge")]
S = {}
for dist in ("uniform", "zipf"):
    for srt in ("qsort", "radix"):
        ser = tm.get((dist, srt, "serial", 1))
        if not ser:
            continue
        T1 = med(ser, "time")
        N = int(ser[0]["N"])
        emit(""); emit(f"## {dist}, local sort = {srt}: time and phase breakdown (median of {nr} runs; N = {N:,})")
        emit("")
        emit("| version | p | total (s) | min (s) | speedup | efficiency | " + " | ".join(n for _, n in phases) +
             " | max/mean count | verified |")
        emit("|---|---|---|---|---|---|" + "---|" * len(phases) + "---|---|")
        emit(f"| serial (1 process) | 1 | {T1:.3f} | {mn(ser, 'time'):.3f} | 1.00 | 1.00 | {T1:.3f} | – | – | – | – | – | "
             f"{'PASS' if all(r['pass'] == '1' for r in ser) else 'FAIL'} |")
        for p in (1, 2, 4):
            rs = tm.get((dist, srt, "parallel", p))
            if not rs:
                continue
            t = med(rs, "time")
            S[(dist, srt, p)] = T1 / t
            emit(f"| sample sort | {p} | {t:.3f} | {mn(rs, 'time'):.3f} | {T1 / t:.2f} | {T1 / t / p:.2f} | " +
                 " | ".join(f"{med(rs, k):.3f}" for k, _ in phases) +
                 f" | {med(rs, 'ratio'):.3f} | {'PASS' if all(r['pass'] == '1' for r in rs) else 'FAIL'} |")

# ---------------- Alltoallv model ----------------
emit(""); emit("## Alltoallv: model (p-1)*alpha + beta*bytes vs measured (median)")
emit("")
emit("| dist | sort | p | max bytes sent by one rank to others | predicted, beta fit (s) | predicted, beta_DRAM (s) | measured (s) |")
emit("|---|---|---|---|---|---|---|")
for (dist, srt, v, p), rs in sorted(tm.items()):
    if v != "parallel" or p == 1:
        continue
    b = med(rs, "maxsent")
    emit(f"| {dist} | {srt} | {p} | {b / 2**20:.1f} MiB | {(p - 1) * alpha + beta * b:.3f} | "
         f"{(p - 1) * alpha + beta_big * b:.3f} | {med(rs, 't_a2a'):.3f} |")

open(f"{OUT}/tables.md", "w").write("\n".join(out) + "\n")
print("\n".join(out))

# ---------------- figures ----------------
fig, axes = plt.subplots(1, 2, figsize=(12, 4.3), sharey=False)
cols = ["tab:blue", "tab:orange", "tab:green", "tab:red", "tab:purple"]
for ax, srt in zip(axes, ("qsort", "radix")):
    labels, bottoms, x = [], [], 0
    xs = []
    for dist in ("uniform", "zipf"):
        for v, p in (("serial", 1), ("parallel", 1), ("parallel", 2), ("parallel", 4)):
            rs = tm.get((dist, srt, v, p))
            if not rs:
                continue
            xs.append((x, dist, v, p, rs))
            labels.append(f"{dist}\n{'serial' if v == 'serial' else f'p={p}'}")
            x += 1
    for i, (k, name) in enumerate(phases):
        vals = [med(rs, k) for (_, _, _, _, rs) in xs]
        base = [sum(med(rs, kk) for kk, _ in phases[:i]) for (_, _, _, _, rs) in xs]
        ax.bar([xx[0] for xx in xs], vals, bottom=base, color=cols[i], label=name)
    ax.set_xticks([xx[0] for xx in xs]); ax.set_xticklabels(labels, fontsize=7.5)
    ax.set_ylabel("time (s), barrier-to-barrier")
    ax.set_title(f"CS5 phase breakdown, local sort = {srt}, N = 1e8 (median of {nr} runs)")
    ax.grid(axis="y", alpha=0.3)
axes[0].legend(fontsize=8)
fig.tight_layout(); fig.savefig(f"{OUT}/phases.png", dpi=200); fig.savefig(f"{OUT}/phases.pdf")

fig, ax = plt.subplots(figsize=(6, 4))
ax.loglog(sizes, [tbest[m] * 1e6 for m in sizes], "o", label="measured (one-way)")
ax.loglog(sizes, [(alpha + beta * m) * 1e6 for m in sizes], "-", label=f"alpha + beta m (alpha={alpha * 1e6:.2f} us, {1 / beta / 1e9:.1f} GB/s)")
ax.set_xlabel("message size (bytes)"); ax.set_ylabel("one-way time (us)"); ax.set_title(f"CS5 ping-pong, 2 ranks (best of {len(pp[sizes[0]])} rounds)")
ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=8)
fig.tight_layout(); fig.savefig(f"{OUT}/pingpong.png", dpi=200)

fig, ax = plt.subplots(figsize=(6, 4))
for key, col in (((4, "zipf", 0), "tab:red"), ((4, "zipf", 1), "tab:blue"), ((4, "uniform", 1), "tab:green"),
                 ((2, "zipf", 0), "salmon"), ((2, "zipf", 1), "lightblue")):
    if key in ovr:
        o = sorted(ovr[key])
        ax.semilogx(o, [float(ovr[key][x]["ratio"]) for x in o], "o-", color=col,
                    label=f"p={key[0]} {key[1]}, {'key+rank+pos' if key[2] else 'key only'}")
ax.axhline(1, ls=":", color="grey")
ax.set_xlabel("oversampling factor ov (ov*p - 1 samples per rank)"); ax.set_ylabel("max / mean elements per rank")
ax.set_title("CS5 load balance after redistribution (N = 1e8, 1 run each:\ncounts are deterministic for a given seed)", fontsize=10); ax.grid(alpha=0.3, which="both"); ax.legend(fontsize=7.5)
fig.tight_layout(); fig.savefig(f"{OUT}/oversampling.png", dpi=200)
print(f"\nwrote {OUT}/tables.md, phases.png, pingpong.png, oversampling.png")
