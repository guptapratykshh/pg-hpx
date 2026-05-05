# Baseline — 2026-05-05 — master @ 2e0fcc123d

Captured immediately after the GSoC follow-up PRs landed on master:
#7244 (post-stdexec cleanup + async_mpi P2300), #7246 (CMake FetchContent
deprecation fix), #7247 (`__loop_` isolation in `make_future`).

## Environment

| Field | Value |
|---|---|
| Master tip | `2e0fcc123d` |
| Date (UTC) | 2026-05-05 |
| OS | Darwin 25.3.0 arm64 |
| CPU | Apple M2 |
| Compiler | Apple clang version 17.0.0 (clang-1700.6.3.2) |
| Build type | `Release` |
| Allocator | `HPX_WITH_MALLOC=system` |
| HPX threads | 4 |

## Methodology

Each benchmark was run 5 times back-to-back with the flags listed in the
"Command" column. The reported number is the **median** of the 5 runs; the
"min — max" column shows the spread. Full raw output of each run is in
`raw/<benchmark>.run<N>.txt`.

---

## 1. `future_overhead_report` — `make_future` round-trip cost

**Command:** `future_overhead_report_test --hpx:threads=4 --futures=500000 --test-all`

| Metric | Median | Min — Max | Unit |
|---|---|---|---|
| `future overhead - create_thread_hierarchical - latch` (avg) | **4.1564e-02** | 3.9243e-02 — 4.2578e-02 | seconds (total for 500000 futures) |

Sorted samples:
```
3.9243e-02
3.9757e-02
4.1564e-02   <- median
4.2444e-02
4.2578e-02
```

This corresponds to a **per-future cost of ~83 ns** (median 0.041564 s ÷ 500000).

---

## 2. `async_overheads` — sequential vs hierarchical task spawn

**Command:** `async_overheads_test --hpx:threads=4`

| Metric | Median | Min — Max | Unit |
|---|---|---|---|
| Elapsed sequential time (total) | **0.000140083** | 0.000133292 — 0.000147959 | seconds |
| Elapsed hierarchical time (total) | **0.000195708** | 0.000183750 — 0.000209458 | seconds |
| Speedup ratio (seq/hier) | ~0.71 | 0.636 — 0.777 | dimensionless |

Sorted sequential samples:
```
0.000133292
0.000137292
0.000140083   <- median
0.000141791
0.000147959
```

Sorted hierarchical samples:
```
0.000183750
0.000190333
0.000195708   <- median
0.000198875
0.000209458
```

(Default test parameters: 128 tasks, 16 sub-tasks per spawn, spread 2, no delay.)

---

## 3. `foreach_scaling` — bulk algorithm overhead

**Command:** `foreach_scaling_test --hpx:threads=4 --enable_all`

| Metric | Median | Min — Max | Unit |
|---|---|---|---|
| Average parallel `for_each` execution time | **1.6278e-05** | 1.5957e-05 — 2.4703e-05 | seconds |

Sorted samples:
```
1.5957e-05
1.6020e-05
1.6278e-05   <- median
2.4525e-05
2.4703e-05
```

Note the bimodal distribution (3 fast runs ~16 µs, 2 slower runs ~24 µs) —
likely first-run thread-pool warmup variance. Reported median is reasonable;
when comparing against this baseline, treat differences below ~50% as noise
unless reproducible across many runs.

(Default test parameters: vector_size=1000, work_delay=1ns, test_count=100.)

---

## Notes for future comparisons

- **Same machine, same build flags** — comparisons are only meaningful on the
  same hardware/compiler combination. If the comparison machine is different,
  capture a new baseline on that machine first.
- **5 runs is light** — for tight regression claims (<5%), use 20+ runs and
  report mean+stdev. 5 runs catches large regressions reliably.
- **Bimodal foreach_scaling** — if you see this pattern in a comparison run,
  it's pre-existing noise, not your refactor.
