#!/usr/bin/env bash
# bench.sh - all runs for Case Study 5. Results go to results/raw.log
#
# Usage: ./bench.sh [rounds]        default 3 rounds (about 20 min on the 2-core laptop)
#   N=100000000 COOL=2 MPIRUN4="mpirun --oversubscribe" ./bench.sh
set -euo pipefail
R=${1:-3}
N=${N:-100000000}
COOL=${COOL:-2}
MPIRUN4=${MPIRUN4:-"mpirun --map-by :OVERSUBSCRIBE"}
OUT=results
mkdir -p "$OUT"
LOG="$OUT/raw.log"
: > "$LOG"

make -s all
launch() { local p=$1; shift; if [ "$p" -le 2 ]; then mpirun --bind-to core -np "$p" "$@"; else $MPIRUN4 -np "$p" "$@"; fi; }
log() { local tag=$1; shift; "$@" | grep '^RESULT' | sed "s/^/$tag /" >> "$LOG"; }

{
  echo "date: $(date)"
  lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|L1d|L2|L3'
  free -h | grep Mem
  gcc --version | head -1
  ompi_info 2>/dev/null | grep -E 'Open MPI:' || true
  echo "N=$N rounds=$R; p<=2: mpirun --bind-to core ; p=4: $MPIRUN4"
} > "$OUT/machine.txt"

# ---- 1. ping-pong (alpha, beta) ----
echo "ping-pong"
for r in 1 2 3; do log "exp=pingpong round=$r" mpirun --bind-to core -np 2 ./pingpong; done

# ---- 2. load balance vs oversampling (counts are deterministic: one run each, radix for speed) ----
echo "oversampling sweep"
for p in 2 4; do for d in uniform zipf; do for t in tie notie; do for ov in 1 4 16 64 256; do
  log "exp=oversample round=1" launch $p ./parallel $N $d $ov radix $t
done; done; done; done

# ---- 3. timing: qsort (as specified) and radix, serial vs p = 1, 2, 4, interleaved ----
for r in $(seq 1 "$R"); do
  for d in uniform zipf; do for s in qsort radix; do
    echo "round $r/$R: $d $s"
    log "exp=time round=$r" ./serial $N $d $s; sleep "$COOL"
    for p in 1 2 4; do log "exp=time round=$r" launch $p ./parallel $N $d 1 $s tie; sleep "$COOL"; done
  done; done
done
echo "done -> $LOG   (now run: python3 analyze.py)"
