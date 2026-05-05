# Design: Run-Loop Bridging — Scoping & Plan

**Status:** Draft for mentor review
**Author:** Pratyksh Gupta (`@guptapratykshh`)
**Target:** GSoC 2026 — Phase II §3 ("Run-Loop Bridging") of the
"Modernizing HPX: Architectural stdexec Migration & Subsystem Integration"
project
**Branch where this doc lives:** `design/run-loop-bridging-investigation`
**Master tip surveyed:** `2e0fcc123d` (post-#7244, post-#7246, post-#7247)

---

## 1. What the proposal asks for

From the GSoC proposal, Phase II §3:

> *Run-Loop Bridging: I will work on the logic that connects the HPX runtime's
> main loop with the stdexec scheduler. The goal here is "zero-overhead task
> steering", ensuring that when a task is ready, it goes directly to the HPX
> thread pool without passing through unnecessary intermediate pointers.*

This was always the most under-specified item in the proposal. This doc
turns it into a concrete plan after surveying the actual code on
master `2e0fcc123d`.

---

## 2. Key finding from the survey

**HPX's primary scheduler does not use `stdexec::run_loop`.**

The HPX thread-pool scheduler is
[`hpx::execution::experimental::thread_pool_policy_scheduler<Policy>`](../../libs/core/executors/include/hpx/executors/thread_pool_scheduler.hpp#L118)
(aliased as `thread_pool_scheduler` for `hpx::launch`). It already fully
satisfies `stdexec::scheduler`:

- Member `schedule()` ([line 518](../../libs/core/executors/include/hpx/executors/thread_pool_scheduler.hpp#L518))
- `tag_invoke(schedule_t, ...)` overloads ([lines 523-535](../../libs/core/executors/include/hpx/executors/thread_pool_scheduler.hpp#L523))
- P3826R5 domain customization via `thread_pool_domain<Policy>` ([lines 85-115](../../libs/core/executors/include/hpx/executors/thread_pool_scheduler.hpp#L85))

`stdexec::run_loop` is used in HPX in exactly **two** places:

1. **`make_future.hpp`** — to drive the
   `future_data_with_run_loop::execute_deferred()` path. The `__loop_`
   private-member access here was just isolated to a single helper in #7247.
2. **The `algorithm_run_loop.cpp` test** — purely a test consumer.

So "Run-Loop Bridging" as literally written ("connect the HPX runtime's main
loop with the stdexec scheduler") is **not the actual problem**. The HPX
thread pool is already wired to stdexec via `thread_pool_scheduler`, with no
intermediate `run_loop`.

What the proposal *meant* — and what we should actually deliver — is **task
steering between standard execution (stdexec sender chains) and the HPX
thread pool, with no avoidable indirection**.

---

## 3. Where indirection actually lives today

### 3.1 The schedule → start → enqueue path

Tracing `stdexec::schedule(thread_pool_sched) | then(...) | sync_wait`:

```
schedule(sched)
   → sender<thread_pool_policy_scheduler>                  (line 423-432)
       .connect(receiver)
   → operation_state<Scheduler, Receiver>                   (line 356)
       (HPX_NO_UNIQUE_ADDRESS members; no padding cost)
       .start()
   → scheduler.execute([this]() { set_value(HPX_MOVE(receiver)); })   (line 381)
   → post_policy_dispatch<Policy>::call(policy, desc, pool, f)
   → threads::register_work(thread_init_data, pool)
       (enqueues onto HPX thread pool)
```

This path is **already pretty tight**. `HPX_NO_UNIQUE_ADDRESS` removes
sender-state padding, the `start()` lambda captures only `this`, and
`post_policy_dispatch` is template-specialized (no vtable). The actual
runtime cost is the `register_work` itself — which is unavoidable.

### 3.2 The three real opportunities

Despite the above, the survey identified three places where overhead
could be reduced:

| # | Opportunity | Cost today | Where |
|---|---|---|---|
| **A** | `continues_on(sender, sched)` always inserts a fresh op-state layer, even when the sender is already on `sched` | One extra operation_state per `continues_on` | [`scheduler_executor.hpp:124-126, 140-142, 165`](../../libs/core/executors/include/hpx/executors/scheduler_executor.hpp#L124) |
| **B** | `thread_pool_bulk_sender` pre-connects the child sender to a `bulk_receiver` inside its own op-state, creating an op-state-within-an-op-state | Type-bloat + one nested op-state | [`thread_pool_scheduler_bulk.hpp:775-833`](../../libs/core/executors/include/hpx/executors/thread_pool_scheduler_bulk.hpp#L775) |
| **C** | `make_future` still routes through the `__loop_` helper, even though we now own the helper | One pointer dereference (currently isolated) | [`make_future.hpp:52-60`](../../libs/core/execution/include/hpx/execution/algorithms/make_future.hpp#L52) |

---

## 4. Recommended scope for the GSoC deliverable

Pick **A and B**. Skip C for now (it's blocked on stdexec upstream).

### 4.1 Item A — `continues_on` elision via domain customization

When user code writes:

```cpp
auto s = schedule(thread_pool_sched)
       | then(...)
       | continues_on(thread_pool_sched);   // <-- redundant: already on it
```

P2300's `continues_on` always wraps the upstream sender in a fresh op-state
that re-schedules onto the destination. But if the upstream sender's
completion scheduler is already equal to the destination, the wrap is a
no-op semantically — the completion callback could be called inline.

**Proposed fix:** add a `transform_sender` overload to `thread_pool_domain`
that detects `continues_on_t` with matching source/destination scheduler and
returns the upstream sender unchanged.

**Estimated size:** ~40 lines in `thread_pool_scheduler.hpp` + 1 unit test.

**Validation:** baseline benchmark from
[`perf-baselines/results-2026-05-05-master-2e0fcc1.md`](../../perf-baselines/results-2026-05-05-master-2e0fcc1.md)
for `future_overhead_report` and `async_overheads`. Expected: no regression
on the unaffected paths, measurable reduction on synthetic `continues_on`-heavy
chains.

### 4.2 Item B — flatten `thread_pool_bulk_sender` op-state nesting

Currently:

```cpp
struct operation_state {
    NestedOpState op_state;       // <-- the connected child sender
    bulk_receiver<...> recv;
    ...
};
```

The `bulk_receiver` is connected to the child sender at sender-construction
time, producing an op-state that lives inside the bulk op-state. This is
type-bloat and forces an extra indirection on every `start()`.

**Proposed fix:** store the receiver directly and connect lazily at `start()`
time, or restructure so the child sender's op-state IS the bulk op-state's
storage (`HPX_NO_UNIQUE_ADDRESS` placement).

**Estimated size:** ~50 lines in `thread_pool_scheduler_bulk.hpp` + careful
review of error-path semantics.

**Validation:** the merged #7240 test
[`executor_algorithm_bulk.cpp`](../../libs/core/executors/tests/unit/executor_algorithm_bulk.cpp)
must continue to pass. Plus add a new microbenchmark exercising
`bulk_chunked` directly.

### 4.3 Item C — make_future `__loop_` full removal (DEFER)

Three options ranked elsewhere in this project:
1. Upstream PR to NVIDIA/stdexec exposing `run_loop::scheduler::get_run_loop()` — best long-term, blocked on review timeline.
2. HPX-owned `run_loop` wrapper that satisfies stdexec's full scheduler concept — attempted in [the failed wrapper attempt during #7247 development]; substitution failure on `sync_wait`'s `__recurse_query_t` confirmed it requires forwarding ~6 internal env queries plus a custom sender type. Estimated ~150 LoC + concept-compliance review with mentors.
3. Status quo: keep the isolated helper from #7247.

**Recommendation:** open the upstream PR in parallel during the GSoC
period. If it lands, we win. If it doesn't, do nothing — the helper from
#7247 is already documented and contained.

---

## 5. Out-of-scope clarifications

To make sure this doc commits to a definite scope, here's what is **NOT**
in this deliverable:

- No changes to `stdexec::run_loop` itself (it's an upstream library).
- No changes to `thread_pool_scheduler`'s public API surface — internal-only optimizations only.
- No new public sender adapters or CPOs.
- No HPX runtime architectural changes (the runtime ↔ thread-pool wiring stays as-is).

---

## 6. Open questions for @hkaiser and @isidorostsa

1. **Is "Item A" (`continues_on` elision) something HPX wants?** Some teams
   prefer the explicit `continues_on` for serialization-of-completion
   guarantees even when redundant. If you'd rather keep it explicit, A is
   off the table and the deliverable shrinks to just B.

2. **For "Item B", do we have a stable invariant** that the child sender's
   op-state can be safely co-located with the bulk op-state's storage?
   i.e., that no other code holds a pointer/reference to the inner op-state
   at the time we'd flatten it.

3. **For "Item C"**, does HPX have a relationship with NVIDIA/stdexec
   that would smooth a small upstream PR adding `run_loop::scheduler::get_run_loop()`?
   If yes, I'm happy to draft it.

4. **Is there an existing "task-steering microbenchmark"** in the
   codebase (something that exercises `schedule() | then() | continues_on() |
   sync_wait()` in a tight loop)? If not, I'll add one as part of this
   deliverable so we have a measurement target. The existing
   `future_overhead_report`/`async_overheads`/`foreach_scaling` baselines
   from [perf-baselines/](../../perf-baselines/) cover adjacent ground but
   not this exact path.

---

## 7. Estimated timeline within Phase II §3 (Weeks 7-8)

| Week | Day | Work |
|---|---|---|
| 7 | 1-2 | Address open questions in §6 with mentors. Lock scope. |
| 7 | 3-5 | Implement Item A + microbenchmark. Land PR. |
| 8 | 1-3 | Implement Item B. Land PR. |
| 8 | 4-5 | Run baselines vs results, write up findings. |

If only B is approved (A rejected per §6 Q1), shift Week 7 to extra B
testing + C upstream-PR exploration.

---

## 8. Definition of done

- A and/or B implemented and merged.
- New microbenchmark in `tests/performance/local/` exercising tight
  schedule chains.
- Numbers from new microbenchmark captured in `perf-baselines/` showing
  measurable improvement over the current 2026-05-05 baseline.
- Single migration-guide section drafted (for the eventual Phase III §3
  documentation deliverable) explaining "what changed for users of
  `bulk` and `continues_on`".
