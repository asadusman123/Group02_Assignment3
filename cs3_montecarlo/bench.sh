#!/usr/bin/env bash
# bench.sh - timing runs for Case Study 3.
#
# Usage:  ./bench.sh [rounds] [M]          defaults: 9 rounds, M = 1e8
#         COOL=10 ./bench.sh               seconds of cool-down between runs
#
# Runs are INTERLEAVED (serial, p=1, p=2, p=4, serial, p=1, ...) so that slow
# drift in CPU clock (turbo, throttling, background load) hits every
# configuration equally instead of biasing whichever ran last.
set -euo pipefail

R=${1:-9}
M=${2:-100000000}
SEED=42
PS=${PS:-"1 2 4"}
COOL=${COOL:-5}
OUT=results
mkdir -p "$OUT"
# One thread per PHYSICAL core first: on this machine logical CPUs 0,1 share
# core 0 and 2,3 share core 1, so without OMP_PLACES=cores "p=2" would put
# both threads on the same core (hyper-threads).
export OMP_PLACES=cores OMP_PROC_BIND=close

# turn a RESULT line into "p,M,price,se,time"
parse() { sed -n 's/^RESULT .* p=\([0-9]*\) M=\([0-9]*\) .*price=\([^ ]*\) se=\([^ ]*\) time=\([^ ]*\).*/\1,\2,\3,\4,\5/p'; }

make -s all

# ---- machine / toolchain info for the report ----
{
  echo "date:     $(date)"
  lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|Socket\(s\)'
  echo; lscpu -e; echo
  free -h | grep Mem
  gcc --version | head -1
  echo "OMP_PLACES=$OMP_PLACES OMP_PROC_BIND=$OMP_PROC_BIND  rounds=$R  M=$M  cool-down=${COOL}s"
} > "$OUT/machine.txt"

# ---- 1. strong scaling (fixed M) ----
CSV="$OUT/strong.csv"
echo "round,version,p,M,price,se,time" > "$CSV"
for r in $(seq 1 "$R"); do
  echo "round $r/$R"
  echo "$r,serial,$(./serial "$M" $SEED | parse)" >> "$CSV"; sleep "$COOL"
  for p in $PS; do
    echo "$r,parallel,$(OMP_NUM_THREADS=$p ./parallel "$M" $SEED jump | parse)" >> "$CSV"
    sleep "$COOL"
  done
done

# ---- 2. fixed-time runs: give each p the M it can finish in the serial best time ----
# M_p = M * T_serial,min / T_p,min  (minimum = least-interference run), 3 interleaved rounds
CSV2="$OUT/fixed_time.csv"
echo "round,version,p,M,price,se,time" > "$CSV2"
declare -A MP
for p in $PS; do
  MP[$p]=$(python3 - "$CSV" "$p" "$M" <<'PY'
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
p, M = int(sys.argv[2]), int(sys.argv[3])
t1 = min(float(r["time"]) for r in rows if r["version"] == "serial")
tp = min(float(r["time"]) for r in rows if r["version"] == "parallel" and int(r["p"]) == p)
print(int(M * t1 / tp) // 2 * 2)
PY
)
done
for r in 1 2 3; do
  for p in $PS; do
    echo "fixed-time round $r p=$p M=${MP[$p]}"
    echo "$r,parallel,$(OMP_NUM_THREADS=$p ./parallel "${MP[$p]}" $SEED jump | parse)" >> "$CSV2"
    sleep "$COOL"
  done
done

# ---- 3. the two broken RNG strategies (critical analysis Q1) ----
PMAX=$(echo $PS | awk '{print $NF}')
CSV3="$OUT/rng_modes.csv"
echo "mode,p,M,price,se,time" > "$CSV3"
for mode in jump shared sameseed; do
  Mm=$M; [ "$mode" = shared ] && Mm=$((M / 5))      # shared is slow; use M/5
  echo "rng mode $mode"
  echo "$mode,$(OMP_NUM_THREADS=$PMAX ./parallel "$Mm" $SEED $mode | parse)" >> "$CSV3"
  sleep "$COOL"
done

echo "done -> $OUT/   (now run: python3 analyze.py)"
