/*
 * pingpong.c - point-to-point latency/bandwidth between rank 0 and rank 1 (Case Study 5, Q3).
 *
 * Usage: mpirun --bind-to core -np 2 ./pingpong
 *
 * For message sizes 1 B ... 64 MiB, rank 0 sends m bytes to rank 1, which
 * sends them back; one-way time = round-trip / 2. Each size: warm-up, then a
 * repetition count chosen to take ~0.2 s; best of 3 trials.
 * Fit (in analyze.py): T(m) = alpha + beta * m.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int rank, p;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    if (p != 2) { if (!rank) fprintf(stderr, "run with exactly 2 ranks\n"); MPI_Finalize(); return 1; }
    const size_t maxm = 64u << 20;
    char *buf = malloc(maxm);
    memset(buf, 1, maxm);

    for (size_t m = 1; m <= maxm; m *= 4) {
        int reps = (int)(2e8 / (double)(m + 1000));          /* rough: ~0.2 s at a few GB/s */
        if (reps < 5) reps = 5;
        if (reps > 20000) reps = 20000;
        double best = 1e30;
        for (int trial = 0; trial < 4; trial++) {           /* trial 0 = warm-up */
            MPI_Barrier(MPI_COMM_WORLD);
            const double t0 = MPI_Wtime();
            for (int r = 0; r < reps; r++) {
                if (rank == 0) {
                    MPI_Send(buf, (int)m, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
                    MPI_Recv(buf, (int)m, MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                } else {
                    MPI_Recv(buf, (int)m, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    MPI_Send(buf, (int)m, MPI_BYTE, 0, 0, MPI_COMM_WORLD);
                }
            }
            const double t = (MPI_Wtime() - t0) / reps / 2.0;
            if (trial > 0 && t < best) best = t;
        }
        if (rank == 0) {
            printf("%10zu B  one-way %10.3f us  %8.3f GB/s\n", m, best * 1e6, m / best / 1e9);
            printf("RESULT pingpong bytes=%zu oneway=%.9e\n", m, best);
        }
    }
    free(buf);
    MPI_Finalize();
    return 0;
}
