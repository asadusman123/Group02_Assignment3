#!/usr/bin/env bash
# bench.sh - all timing and correctness runs for Case Study 2.
#
# Usage: ./bench.sh [rounds]            default 7 rounds;  COOL=2 ./bench.sh  (cool-down seconds)
#
# Every run's RESULT line is appended to results/raw.log, tagged with the
# experiment and round. analyze.py turns that log into tables and graphs.
# Timing runs are interleaved across configurations within each round.
set -euo pipefail
R=${1:-7}
COOL=${COOL:-1}
L=${L:-100000000}
SEED=2026
PS="1 2 4"
OUT=results
mkdir -p "$OUT"
LOG="$OUT/raw.log"
: > "$LOG"

# threads on separate PHYSICAL cores first (p=2 -> one thread per core)
export OMP_PLACES=cores OMP_PROC_BIND=close

make -s all

{
  echo "date:     $(date)"
  lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|L1d|L2|L3'
  echo; lscpu -e; echo
  free -h | grep Mem
  gcc --version | head -1
  echo "OMP_PLACES=$OMP_PLACES OMP_PROC_BIND=$OMP_PROC_BIND rounds=$R L=$L cool-down=${COOL}s"
} > "$OUT/machine.txt"

run() {   # run <tag> <command...>
  local tag="$1"; shift
  "$@" | grep '^RESULT' | sed "s/^/$tag /" >> "$LOG"
}

# ---- 1. correctness (L = 1e8, verify against the serial histogram) ----
echo "correctness runs"
for d in uniform skewed; do for k in 8 12; do
  run "exp=correct round=0" ./serial $k $L $d $SEED
  for v in A B C N; do for p in 2 4; do
    run "exp=correct round=0" env OMP_NUM_THREADS=$p ./parallel $v $k $L $d $SEED verify
  done; done
done; done

# ---- 2. memory probe ----
echo "memory probe"
for r in 1 2 3; do for p in $PS; do
  OMP_NUM_THREADS=$p ./membench | grep '^RESULT' | sed "s/^/exp=membench round=$r /" >> "$LOG"
  sleep "$COOL"
done; done

# ---- 3. strong scaling + 4. false sharing, interleaved per round ----
for r in $(seq 1 "$R"); do
  echo "timing round $r/$R"
  for d in uniform skewed; do for k in 8 12; do
    run "exp=strong round=$r" ./serial $k $L $d $SEED; sleep "$COOL"
    for v in B C; do for p in $PS; do
      run "exp=strong round=$r" env OMP_NUM_THREADS=$p ./parallel $v $k $L $d $SEED noverify
      sleep "$COOL"
    done; done
  done; done
  for v in fs fspad; do for p in $PS; do
    run "exp=falseshare round=$r" env OMP_NUM_THREADS=$p ./parallel $v 1 $L uniform $SEED noverify
    sleep "$COOL"
  done; done
done
echo "done -> $LOG   (now run: python3 analyze.py)"
