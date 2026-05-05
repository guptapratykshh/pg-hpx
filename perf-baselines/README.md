# Performance Baselines

This directory captures HPX benchmark numbers at known points on master so that
later refactors (in particular the GSoC 2026 work on `make_future`/`keep_future`
boundaries, run-loop bridging, and async_mpi/async_cuda native sender migration)
can be measured against a stable reference.

Each capture event lives in its own file `results-YYYY-MM-DD-master-<sha>.md`
with the median of 5 runs per metric, plus the full raw output of every run
under `raw/`.

## How to compare against a baseline

1. On a refactor branch, build the same three benchmark targets in `Release`:
   ```
   cmake --build build --target future_overhead_report_test \
                                async_overheads_test \
                                foreach_scaling_test
   ```
2. Run each 5x with the same flags listed in the baseline file.
3. Compute the median of the 5 runs per metric.
4. Compare against the baseline median; flag any regression > ~5% for review.

## Files in this directory

- `README.md` — this file
- `results-<date>-master-<sha>.md` — one per capture event
- `raw/` — full stdout/stderr of each individual run

## Why these three benchmarks

| Benchmark | What it measures | Relevant to which GSoC item |
|---|---|---|
| `future_overhead_report` | `make_future` round-trip cost via `create_thread_hierarchical` | Phase II §1 (make_future native rewrite) |
| `async_overheads` | Sequential vs hierarchical task spawn cost | Phase II §3 (run-loop bridging) |
| `foreach_scaling` | Bulk algorithm overhead through the sender path | Phase III §1 (async_mpi/async_cuda sender integration via thread_pool_scheduler bulk) |
