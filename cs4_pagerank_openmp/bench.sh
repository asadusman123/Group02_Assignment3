#!/usr/bin/env bash
# bench.sh - all runs for Case Study 4. Results go to results/raw.log
# Usage: ./bench.sh [rounds]     default 5 rounds;  COOL=1 ./bench.sh;  SCALE=20
set -euo pipefail
R=${1:-5}
COOL=${COOL:-1}
SCALE=${SCALE:-20}
G=graph_s$SCALE.bin
OUT=results
mkdir -p "$OUT"
LOG="$OUT/raw.log"
: > "$LOG"
export OMP_PLACES=cores OMP_PROC_BIND=close

make -s all
# (re)generate the graph every time: deterministic (seed 2026), ~5-10 s, and it
# also writes the degree histogram and graph statistics for the report
./gen "$SCALE" 16 2026 "$G" "$OUT/degrees.csv" > "$OUT/graph_info.txt"
grep '^RESULT' "$OUT/graph_info.txt" | sed 's/^/exp=graph /' >> "$LOG"

{
  echo "date: $(date)"
  lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|L1d|L2|L3'
  free -h | grep Mem
  gcc --version | head -1
  echo "OMP_PLACES=$OMP_PLACES OMP_PROC_BIND=$OMP_PROC_BIND rounds=$R graph=$G"
} > "$OUT/machine.txt"

run() { local tag=$1; shift; "$@" | grep '^RESULT' | sed "s/^/$tag /" >> "$LOG"; }

./serial "$G" > /dev/null          # reference ranks for verification

# ---- 1. verification (incl. the racy push at p = 2) ----
for v in pull push pushna; do for p in 1 2 4; do
  run "exp=verify round=0" env OMP_NUM_THREADS=$p OMP_SCHEDULE=static ./parallel $v "$G"
done; done

SWEEP="static static,64 static,1024 dynamic,1 dynamic,16 dynamic,64 dynamic,256 dynamic,1024 dynamic,4096 dynamic,16384 guided guided,64 guided,256"
for r in $(seq 1 "$R"); do
  echo "round $r/$R"
  # ---- 2. strong scaling (3 schedules) ----
  run "exp=strong round=$r" ./serial "$G"; sleep "$COOL"
  for v in pull push; do for s in static dynamic,256 guided; do for p in 1 2 4; do
    run "exp=strong round=$r" env OMP_NUM_THREADS=$p OMP_SCHEDULE=$s ./parallel $v "$G"; sleep "$COOL"
  done; done; done
  # ---- 3. schedule / chunk sweep (pull) ----
  for p in 2 4; do for s in $SWEEP; do
    run "exp=sweep round=$r" env OMP_NUM_THREADS=$p OMP_SCHEDULE=$s ./parallel pull "$G"; sleep "$COOL"
  done; done
  # ---- 4. cost of the atomic itself: push vs push-without-atomic at p = 1 ----
  for v in push pushna; do
    run "exp=atomic round=$r" env OMP_NUM_THREADS=1 OMP_SCHEDULE=static ./parallel $v "$G"; sleep "$COOL"
  done
done
echo "done -> $LOG   (now run: python3 analyze.py)"
