/*
 * parallel.c - distributed sample sort with MPI (Case Study 5).
 *
 * Usage: mpirun -np p ./parallel [N] [uniform|zipf] [ov] [qsort|radix] [tie|notie] [seed]
 *        defaults: N = 1e8, uniform, ov = 1, qsort, tie, seed = 2026
 *
 * Every rank generates its own N/p keys (shard = rank, fixed seed) and never
 * holds more than its share plus what it receives.
 *   1. local sort                         (qsort, or radix with "radix")
 *   2. sampling: s = ov*p - 1 REGULAR samples per rank, at local positions
 *      (i+1)*n/(ov*p), i.e. the local quantiles 1/(ov p), 2/(ov p), ...
 *      -> MPI_Gather to rank 0, which sorts the p*s samples and takes as
 *      splitter j the sample at global quantile (j+1)/p -> MPI_Bcast.
 *      ov = 1 is the classic p-1 samples per rank; ov > 1 = oversampling.
 *   3. partition: binary search of each splitter in the sorted local array
 *   4. redistribution: MPI_Alltoall (counts) + MPI_Alltoallv (keys)
 *   5. final merge: the p received runs are each sorted -> pairwise merges
 *
 * Duplicates. A sample/splitter is the triple (key, rank, position), compared
 * lexicographically ("tie", default). Equal keys are then ordered by where
 * they came from, so a run of identical keys can be split between ranks. With
 * "notie" only the key is used, so all copies of a key must go to one rank
 * (that is what breaks load balance on the Zipf input).
 *
 * Verification: every rank's data sorted; max of all lower ranks <= my min
 * (MPI_Exscan, so empty ranks are handled); total count preserved; multiset
 * fingerprint (sum of hashed keys) identical before and after.
 * Phase times: barrier-to-barrier, so each phase time is the slowest rank's.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <mpi.h>
#include "ss_common.h"

typedef struct { uint32_t key; int32_t rank; uint64_t pos; } sample_t;

static int cmp_sample(const void *p, const void *q)
{
    const sample_t *a = p, *b = q;
    if (a->key != b->key) return a->key < b->key ? -1 : 1;
    if (a->rank != b->rank) return a->rank < b->rank ? -1 : 1;
    return (a->pos > b->pos) - (a->pos < b->pos);
}

/* first index with a[i] >= key / > key */
static uint64_t lower_bound(const uint32_t *a, uint64_t n, uint32_t key)
{
    uint64_t lo = 0, hi = n;
    while (lo < hi) { const uint64_t m = (lo + hi) / 2; if (a[m] < key) lo = m + 1; else hi = m; }
    return lo;
}
static uint64_t upper_bound(const uint32_t *a, uint64_t n, uint32_t key)
{
    uint64_t lo = 0, hi = n;
    while (lo < hi) { const uint64_t m = (lo + hi) / 2; if (a[m] <= key) lo = m + 1; else hi = m; }
    return lo;
}

static void merge2(const uint32_t *a, uint64_t na, const uint32_t *b, uint64_t nb, uint32_t *out)
{
    uint64_t i = 0, j = 0, k = 0;
    while (i < na && j < nb) out[k++] = (b[j] < a[i]) ? b[j++] : a[i++];
    while (i < na) out[k++] = a[i++];
    while (j < nb) out[k++] = b[j++];
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int p, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &p);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    const uint64_t N = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    const int zipf  = argc > 2 && !strcmp(argv[2], "zipf");
    int ov          = argc > 3 ? atoi(argv[3]) : 1;
    const int radix = argc > 4 && !strcmp(argv[4], "radix");
    const int tie   = !(argc > 5 && !strcmp(argv[5], "notie"));
    const uint64_t seed = argc > 6 ? strtoull(argv[6], NULL, 10) : 2026;
    if (ov < 1) ov = 1;

    /* ---- generation (not timed) ---- */
    const uint64_t lo = (uint64_t)rank * N / p, nloc = (uint64_t)(rank + 1) * N / p - lo;
    uint32_t *a = malloc((nloc ? nloc : 1) * sizeof(uint32_t));
    if (!a) { fprintf(stderr, "rank %d: out of memory\n", rank); MPI_Abort(MPI_COMM_WORLD, 1); }
    gen_keys(a, nloc, zipf, seed, rank);
    uint64_t fp_local = fingerprint(a, nloc), fp_before = 0;
    MPI_Allreduce(&fp_local, &fp_before, 1, MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);

    double t[6];          /* timestamps between phases */
    MPI_Barrier(MPI_COMM_WORLD);
    t[0] = MPI_Wtime();

    /* ---- 1. local sort ---- */
    local_sort(a, nloc, radix);
    MPI_Barrier(MPI_COMM_WORLD);
    t[1] = MPI_Wtime();

    /* ---- 2. sampling and splitters ---- */
    const int s = ov * p - 1;
    sample_t *mine = malloc((s ? s : 1) * sizeof(sample_t));
    for (int i = 0; i < s; i++) {
        const uint64_t pos = nloc ? (uint64_t)(i + 1) * nloc / (s + 1) : 0;
        mine[i].key = nloc ? a[pos] : 0;
        mine[i].rank = rank;
        mine[i].pos = pos;
    }
    sample_t *spl = malloc((p > 1 ? p - 1 : 1) * sizeof(sample_t));
    if (p > 1) {
        sample_t *all = NULL;
        if (rank == 0) all = malloc((size_t)p * s * sizeof(sample_t));
        MPI_Gather(mine, s * (int)sizeof(sample_t), MPI_BYTE, all, s * (int)sizeof(sample_t), MPI_BYTE, 0, MPI_COMM_WORLD);
        if (rank == 0) {
            qsort(all, (size_t)p * s, sizeof(sample_t), cmp_sample);
            /* every local quantile q/(ov p) appears p times in the sorted samples, at
             * indices p(q-1) .. pq-1; splitter j uses q = ov(j+1), middle copy */
            for (int j = 0; j < p - 1; j++) spl[j] = all[(size_t)p * (ov * (j + 1) - 1) + p / 2];
            free(all);
        }
        MPI_Bcast(spl, (p - 1) * (int)sizeof(sample_t), MPI_BYTE, 0, MPI_COMM_WORLD);
    }
    free(mine);
    MPI_Barrier(MPI_COMM_WORLD);
    t[2] = MPI_Wtime();

    /* ---- 3. partition: bucket j gets elements <= splitter j (and > splitter j-1) ---- */
    int *scount = calloc(p, sizeof(int)), *sdispl = calloc(p, sizeof(int));
    int *rcount = calloc(p, sizeof(int)), *rdispl = calloc(p, sizeof(int));
    uint64_t prev = 0;
    for (int j = 0; j < p; j++) {
        uint64_t b;
        if (j == p - 1) b = nloc;
        else {
            const uint64_t l = lower_bound(a, nloc, spl[j].key), u = upper_bound(a, nloc, spl[j].key);
            if (!tie || rank < spl[j].rank) b = u;          /* all my copies of the key go left   */
            else if (rank > spl[j].rank)    b = l;          /* all my copies go right             */
            else                            b = spl[j].pos + 1;   /* split my run at the sample  */
            if (b < prev) b = prev;
        }
        if (b - prev > (uint64_t)INT32_MAX) { fprintf(stderr, "bucket too large for MPI int counts\n"); MPI_Abort(MPI_COMM_WORLD, 1); }
        scount[j] = (int)(b - prev);
        sdispl[j] = (int)prev;
        prev = b;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    t[3] = MPI_Wtime();

    /* ---- 4. redistribution ---- */
    MPI_Alltoall(scount, 1, MPI_INT, rcount, 1, MPI_INT, MPI_COMM_WORLD);
    uint64_t nrecv = 0;
    for (int j = 0; j < p; j++) { rdispl[j] = (int)nrecv; nrecv += (uint64_t)rcount[j]; }
    uint32_t *b = malloc((nrecv ? nrecv : 1) * sizeof(uint32_t));
    if (!b) { fprintf(stderr, "rank %d: out of memory for %" PRIu64 " keys\n", rank, nrecv); MPI_Abort(MPI_COMM_WORLD, 1); }
    MPI_Alltoallv(a, scount, sdispl, MPI_UINT32_T, b, rcount, rdispl, MPI_UINT32_T, MPI_COMM_WORLD);
    free(a);
    MPI_Barrier(MPI_COMM_WORLD);
    t[4] = MPI_Wtime();

    /* ---- 5. merge the p sorted runs (pairwise, log2 p passes) ---- */
    uint32_t *tmp = malloc((nrecv ? nrecv : 1) * sizeof(uint32_t));
    uint64_t *rs = malloc((p + 1) * sizeof(uint64_t));    /* run starts */
    int nruns = p;
    for (int j = 0; j < p; j++) rs[j] = (uint64_t)rdispl[j];
    rs[p] = nrecv;
    uint32_t *src = b, *dst = tmp;
    while (nruns > 1) {
        int k = 0;
        for (int r = 0; r < nruns; r += 2) {
            if (r + 1 < nruns) merge2(src + rs[r], rs[r + 1] - rs[r], src + rs[r + 1], rs[r + 2] - rs[r + 1], dst + rs[r]);
            else memcpy(dst + rs[r], src + rs[r], (rs[r + 1] - rs[r]) * sizeof(uint32_t));
            rs[k++] = rs[r];
        }
        rs[k] = nrecv;
        nruns = k;
        uint32_t *x = src; src = dst; dst = x;
    }
    MPI_Barrier(MPI_COMM_WORLD);
    t[5] = MPI_Wtime();
    uint32_t *res = src;

    /* ---- verification (untimed) ---- */
    int ok = is_sorted_u32(res, nrecv);
    uint32_t mymax = nrecv ? res[nrecv - 1] : 0, prevmax = 0;
    MPI_Exscan(&mymax, &prevmax, 1, MPI_UINT32_T, MPI_MAX, MPI_COMM_WORLD);
    if (rank > 0 && nrecv > 0 && prevmax > res[0]) ok = 0;
    uint64_t total = 0, fp_after = 0, maxcount = 0;
    fp_local = fingerprint(res, nrecv);
    MPI_Allreduce(&fp_local, &fp_after, 1, MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&nrecv, &total, 1, MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);
    MPI_Allreduce(&nrecv, &maxcount, 1, MPI_UINT64_T, MPI_MAX, MPI_COMM_WORLD);
    int allok = 0;
    MPI_Allreduce(&ok, &allok, 1, MPI_INT, MPI_LAND, MPI_COMM_WORLD);
    allok = allok && total == N && fp_after == fp_before;

    uint64_t *counts = NULL;
    if (rank == 0) counts = malloc(p * sizeof(uint64_t));
    MPI_Gather(&nrecv, 1, MPI_UINT64_T, counts, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    /* bytes this rank sent to OTHER ranks (for the communication model) */
    uint64_t sent = 0, maxsent = 0;
    for (int j = 0; j < p; j++) if (j != rank) sent += (uint64_t)scount[j] * sizeof(uint32_t);
    MPI_Reduce(&sent, &maxsent, 1, MPI_UINT64_T, MPI_MAX, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        const double ratio = (double)maxcount / ((double)N / p);
        printf("N=%" PRIu64 " p=%d dist=%s ov=%d (%d samples/rank) sort=%s splitters=%s\n", N, p,
               zipf ? "zipf" : "uniform", ov, s, radix ? "radix" : "qsort", tie ? "key+rank+pos" : "key only");
        printf("counts     :");
        for (int j = 0; j < p; j++) printf(" %" PRIu64, counts[j]);
        printf("   max/mean = %.3f\n", ratio);
        printf("verify     : %s (each rank sorted, ranks in order, %" PRIu64 " of %" PRIu64 " keys, fingerprint %s)\n",
               allok ? "PASS" : "FAIL", total, N, fp_after == fp_before ? "equal" : "DIFFERENT");
        printf("phases (s) : local sort %.4f | sampling %.4f | partition %.4f | alltoall(v) %.4f | merge %.4f\n",
               t[1] - t[0], t[2] - t[1], t[3] - t[2], t[4] - t[3], t[5] - t[4]);
        printf("time       : %.4f s  (%.1f M keys/s); max bytes sent by one rank to others: %.1f MiB\n",
               t[5] - t[0], N / (t[5] - t[0]) / 1e6, maxsent / 1048576.0);
        printf("RESULT version=parallel p=%d N=%" PRIu64 " dist=%s sort=%s ov=%d tie=%d time=%.6f t_local=%.6f "
               "t_sample=%.6f t_part=%.6f t_a2a=%.6f t_merge=%.6f maxcount=%" PRIu64 " ratio=%.4f pass=%d maxsent=%" PRIu64 " counts=",
               p, N, zipf ? "zipf" : "uniform", radix ? "radix" : "qsort", ov, tie, t[5] - t[0], t[1] - t[0],
               t[2] - t[1], t[3] - t[2], t[4] - t[3], t[5] - t[4], maxcount, ratio, allok, maxsent);
        for (int j = 0; j < p; j++) printf("%s%" PRIu64, j ? "," : "", counts[j]);
        printf("\n");
        free(counts);
    }
    free(b); free(tmp); free(rs); free(spl);
    free(scount); free(sdispl); free(rcount); free(rdispl);
    MPI_Finalize();
    return 0;
}
