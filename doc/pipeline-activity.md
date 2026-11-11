# Pipeline activity for Alchemist

Include `<StormByte/multimedia/pipeline/telemetry.hxx>` and retain
`job.Telemetry()`. Read `telemetry->Topology()`; each `TopologyStage::Activity`
is an owned value containing `Workers`, not an exclusive aggregate stage state.
`stage.Metrics->Activity()` also samples the clocks directly, including stages
obtained through `JobTelemetry::Stages()`.

This requires Buffer **4.0.0 retagged with `AdmissionWaitObserver`** and
Multimedia **3.3.0 rebuilt against those headers**. The admission policy, queue
limits, buffer sizes and optimization settings have not changed. These are
compatible C++ ABI APIs, not a C wire protocol: rebuild consumers for the new
public layouts and keep Base/Buffer/Multimedia loaded while retaining handles.
Owned vectors/handles use Safe storage; worker snapshots contain only values.
There are no borrowed worker pointers in the returned snapshots.

## What the times mean

Each worker accounts monotonic wall time in exactly one category:

| Category | Observed operation |
|---|---|
| `Preparing` | Stage setup, excluding instrumented waits |
| `Processing` | Inside `Process`, including EOF flush, minus instrumented waits |
| `WaitingInput` | Waiting on a pipeline input/readiness notification or a demux pending list to receive a packet |
| `WaitingOutput` | Actual local-capacity admission wait, full demux pending list, or waiting for demux feeders to drain/join |
| `WaitingBudget` | Actual shared-budget admission wait reported by Buffer |
| `Paused` | Actually blocked in the existing cooperative plan-pause wait |
| `Unknown` | Worker administration outside setup/process and any explicitly unclassified interval |

`Finished` and `Failed` are terminal states, not accumulating categories.
Cancellation normally finishes a worker; genuine stage failure produces
`Failed`. A worker that never started remains `Available == false`, not a
zero-utilization worker.

The sum of the seven durations equals `SampledAt - StartedAt` exactly in
integer nanoseconds. Transitions and snapshots take the same short lock;
snapshots include the currently open interval without modifying the clock.
Nested waits suspend the outer category and restore it afterwards: they do
not also add to processing or to the outer wait. Slow encoder calls therefore
show growing processing time before the call returns.

`SampledAt`, `StartedAt` and `FinishedAt` use the process's steady-clock epoch
in nanoseconds, not UTC. Once execution ends, `SampledAt == FinishedAt` and
all values freeze. The lock protects state, totals and the sample reference
as one value. Different workers/stages/queues are sampled sequentially, not
as one globally atomic cut.

`StageTelemetry::ProcessTotal`, `ProcessCalls`, `WaitTotal`, setup and all
existing counters keep their previous meaning. In particular `ProcessTotal`
still includes blocked output delivery, and records completed calls only.
Do **not** substitute it for the exclusive `Processing` duration.

## Workers, identity and coverage

Use the tuple `(TopologySnapshot.Run, TopologyStage.Id, Worker.Id,
Worker.Generation)` to match samples. Reset the baseline if any component
changes. Worker ids are process-unique; `Generation` advances if a clock is
started again. Stage ids and queue/budget topology are unchanged.

- `ActivityRole::Main` is one pumper thread. Demux, decode, filters, encode,
  remux, mux and inspection decoders use the same instrumentation.
- `ActivityRole::DemuxFeeder` is a separate per-source-track thread;
  `Worker.Track` identifies that track. Demux main-reader activity and feeder
  activity are **not** combined into a fabricated exclusive state.
- `StageActivitySnapshot.Available` means at least one instrumented worker
  started; inspect each worker's `Available` too.
- `Complete == false` means a clock registration could not allocate. The job
  continues, but missing execution domains are unknown. This flag is separate
  from queue/topology bookkeeping's `TopologySnapshot.Complete`.
- `WaitReasons == true` advertises the Buffer cause observer. Buffer reports
  the reason of the actual admission retry: local bounds are checked before
  aggregate reservation. If both deny admission, that interval is local
  capacity; a subsequent retry can change to shared budget. Never infer the
  reason by observing a full queue or subtracting global budget counters.
- `InternalWorkersObserved == false` explicitly excludes codec threads
  (FFmpeg/x265/libvmaf), parallel filter-helper tasks and buffered-I/O service
  threads. Their CPU time and internal scheduling are not known. The outer
  pipeline worker's call can include internal codec work, helper joins,
  device waits or I/O that is not further subdivided.

Pause is cooperative, not a retroactive rewrite of an in-flight call. A
worker executing a codec call continues in `Processing` until it reaches its
pause point; an independently draining feeder/filter may continue working or
waiting. Only an actual plan-pause wait is `Paused`. A UI pause flag does not
make every thread simultaneously paused.

After teardown a `Final` topology keeps activity **by value**, immutable even
if another user continues using a shared budget. Retained metrics also own
their clocks independently of workers. If the final topology allocation
fails, existing `Unfrozen` semantics remain: detached queues/clocks are still
readable and terminal, but the aggregate budget may remain live and topology
`Complete` is false. Snapshot allocation can throw; callers should surface
that failure rather than rendering zero utilization.

## Deltas, not a global efficiency number

Match stage/worker ids, then call this application-side helper for each
worker. The returned array is ordered like the seven categories above.
Do not silently clamp invalid deltas to manufacture a valid percentage.

```cpp
#include <StormByte/multimedia/pipeline/telemetry.hxx>
#include <array>
#include <optional>

namespace P = StormByte::Multimedia::Pipeline;

std::optional<std::array<double, 7>> ActivityPercentages(
	std::uint64_t oldRun, std::uint64_t newRun,
	std::uint64_t oldStage, std::uint64_t newStage,
	const P::WorkerActivitySnapshot& before,
	const P::WorkerActivitySnapshot& after) {
	if (oldRun != newRun || oldStage != newStage ||
		!before.Available || !after.Available ||
		before.Id != after.Id || before.Generation != after.Generation)
		return std::nullopt; // New/unknown execution: start a baseline.

	const auto interval = after.SampledAt - before.SampledAt;
	if (interval.count() <= 0)
		return std::nullopt; // Also normal for two identical final samples.

	const std::array deltas{
		after.Preparing - before.Preparing,
		after.Processing - before.Processing,
		after.WaitingInput - before.WaitingInput,
		after.WaitingOutput - before.WaitingOutput,
		after.WaitingBudget - before.WaitingBudget,
		after.Paused - before.Paused,
		after.Unknown - before.Unknown
	};
	std::array<double, 7> percentages{};
	std::chrono::nanoseconds sum{};
	for (std::size_t i = 0; i < deltas.size(); ++i) {
		if (deltas[i].count() < 0 || deltas[i] > interval)
			return std::nullopt; // Surface an inconsistent sample.
		sum += deltas[i];
		percentages[i] = 100.0 * static_cast<double>(deltas[i].count())
			/ static_cast<double>(interval.count());
	}
	if (sum != interval)
		return std::nullopt;
	return percentages;
}
```

For one worker each percentage is in `[0,100]`, and the sum is 100 within
floating-point rounding. If a UI explicitly aggregates simultaneous workers,
report worker-seconds or divide summed category durations by summed observed
worker intervals and label that denominator. Summing worker percentages can
exceed 100 and must never be shown as one exclusive stage state. There is no
library-provided global pipeline efficiency percentage.

An empty queue measures **waiting entries**, not an empty processor.
Fast audio, mux and sparse subtitles can spend time waiting with empty
queues while behaving correctly. Full video queues indicate normal
backpressure, not failure. Neither condition diagnoses utilization, failure
or a need to change buffering. FFmpeg/x265 internal buffer occupancy and
capacity are not observable here and are intentionally not invented.

## Validation

- Pipe tests hold input, a full output and an external shared-budget lease;
  live samples show the correct open wait and unchanged processing.
- Nested processing/input/output/budget/pause scopes conserve time exactly
  (tolerance **0 ns**) and terminal samples remain identical after delays.
- A gated real audio `Process` exposes the open interval; actual
  pause/resume grows pause time without growing that worker's processing.
- Two concurrent jobs using one externally restricted budget each expose
  budget wait and retain their final samples after destruction.
- Topology readers stress concurrent live snapshots; identity, coverage,
  demux feeder separation, cancellation, failure and retained final values
  are checked.
- VMAF's analytics worker and inspection decoder expose their processing;
  mixed x265/audio-remux/sparse and empty subtitle tests preserve output
  correctness, progressive mux delivery and normal input waits.

Sleep-based tests require at least their deliberately held interval, not a
tight scheduler-dependent upper bound. Timing conservation uses exact tick
arithmetic, not a percentage tolerance.
