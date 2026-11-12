#!/usr/bin/env bash
# bench.sh - all runs for Case Study 1. Results go to results/raw.log
#
# Usage: ./bench.sh            (about 25 min on the 2-core laptop)
#   ROUNDS_SMALL=5 ROUNDS_BIG=3 COOL=2 ./bench.sh
#   MPIRUN4="mpirun --oversubscribe" ./bench.sh     (Open MPI 4 syntax)
#
# p = 1, 2 run with one rank per physical core (--bind-to core);
# p = 4 oversubscribes the 2 physical cores (2 ranks per core, hyper-threads).
set -euo pipefail
RS=${ROUNDS_SMALL:-5}
RB=${ROUNDS_BIG:-3}
COOL=${COOL:-2}
T=${T:-500}
NSMALL=${NSMALL:-1024}
NBIG=${NBIG:-4096}
NWEAK=${NWEAK:-2048}
MPIRUN4=${MPIRUN4:-"mpirun --map-by :OVERSUBSCRIBE"}
OUT=results
mkdir -p "$OUT"
LOG="$OUT/raw.log"
: > "$LOG"

make -s all

launch() {   # launch <p> <args...>
  local p=$1; shift
  if [ "$p" -le 2 ]; then mpirun --bind-to core -np "$p" "$@"; else $MPIRUN4 -np "$p" "$@"; fi
}
log() {      # log <tag> <command...>
  local tag=$1; shift
  "$@" | grep '^RESULT' | sed "s/^/$tag /" >> "$LOG"
}

{
  echo "date: $(date)"
  lscpu | grep -E 'Model name|^CPU\(s\)|Thread\(s\) per core|Core\(s\) per socket|L1d|L2|L3'
  echo; lscpu -e; echo
  free -h | grep Mem
  gcc --version | head -1
  ompi_info 2>/dev/null | grep -E 'Open MPI:' || mpirun --version 2>/dev/null | head -1 || true
  echo "T=$T  strong N=$NSMALL ($RS rounds), N=$NBIG ($RB rounds); weak N0=$NWEAK; cool-down ${COOL}s"
  echo "p<=2: mpirun --bind-to core ; p=4: $MPIRUN4"
} > "$OUT/machine.txt"

# ---- 1. verification: final grid must be bit-identical to serial ----
echo "verification"
./serial $NSMALL $T 50 /tmp/heat_serial.bin > /dev/null
for d in 1d 2d; do for m in blocking nonblocking nbtest; do for p in 1 2 4; do
  launch $p ./parallel $NSMALL $T 50 $d $m /tmp/heat_par.bin > /dev/null
  if cmp -s /tmp/heat_serial.bin /tmp/heat_par.bin; then r=IDENTICAL; else r=DIFFERENT; fi
  echo "exp=verify decomp=$d mode=$m p=$p N=$NSMALL T=$T result=$r" >> "$LOG"
done; done; done
rm -f /tmp/heat_serial.bin /tmp/heat_par.bin

# configurations: at p = 1 and 2 MPI_Dims_create gives 1x1 / 2x1 for both 1d and 2d,
# so only p = 4 distinguishes the decompositions (4x1 vs 2x2).
strong_round() {   # strong_round <N> <round>
  local N=$1 r=$2
  log "exp=strong round=$r" ./serial $N $T 50; sleep "$COOL"
  for m in blocking nonblocking nbtest; do
    for p in 1 2; do log "exp=strong round=$r" launch $p ./parallel $N $T 50 2d $m; sleep "$COOL"; done
    for d in 1d 2d; do log "exp=strong round=$r" launch 4 ./parallel $N $T 50 $d $m; sleep "$COOL"; done
  done
}

# ---- 2. strong scaling ----
for r in $(seq 1 "$RS"); do echo "strong N=$NSMALL round $r/$RS"; strong_round $NSMALL $r; done
for r in $(seq 1 "$RB"); do echo "strong N=$NBIG round $r/$RB"; strong_round $NBIG $r; done

# ---- 3. convergence-check cost: K = 1 vs K = 50 ----
for r in $(seq 1 "$RB"); do
  echo "K comparison round $r/$RB"
  for N in $NSMALL $NBIG; do for K in 1 50; do
    log "exp=kcost round=$r" ./serial $N $T $K; sleep "$COOL"
    for p in 2 4; do log "exp=kcost round=$r" launch $p ./parallel $N $T $K 2d nonblocking; sleep "$COOL"; done
  done; done
done

# ---- 4. weak scaling: N_p = N0 * sqrt(p) keeps cells per rank constant ----
for r in $(seq 1 "$RB"); do
  echo "weak scaling round $r/$RB"
  for p in 1 2 4; do
    Np=$(python3 -c "import math; print(int(round($NWEAK * math.sqrt($p))))")
    log "exp=weak round=$r" launch $p ./parallel $Np $T 50 2d nonblocking; sleep "$COOL"
    log "exp=weak round=$r" ./serial $Np $T 50; sleep "$COOL"
  done
done
echo "done -> $LOG   (now run: python3 analyze.py)"
