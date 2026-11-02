# Exception-boundary reconciliation

## Result and provenance

**Superseding allocation result:** all 251 archived barrier/bounded-formatting
emissions below have current treatments in [oom-batch](oom-batch/README.md);
the twelve pending constructor/report/subtitle sites are committed and
validated. The narrative below preserves the original review and its source
snapshot, not a current unresolved-barrier count. Semantic oversized-input
risks and final analyzer warnings remain explicitly documented.

All **307** archived exception/new emissions now have a concrete symbol/contract disposition:
**238 source-confirmed surviving local noexcept barriers**,
**12 worktree boundary changes pending completion/validation**,
**10 committed boundary treatments**, **13 bounded-output implementation/portability risks**,
**2 proved guarded/nonallocating bodies**, and **32 classified test/harness emissions**.
No `RESIDUAL_EXCEPTION_SYMBOL_UNRECONCILED` category remains.

This is **not an OOM all-clear** and does not mean 238 independent bugs: the sites share the grouped causes below. Direct source proof means a throwing owner/container/message operation is evaluated under a surviving local noexcept without an encompassing catch; no new fault injection was performed by this read-only triage. The archived stdlib exception name does not prove a realizable length overflow; ordinary allocation exhaustion is an independent failure mode of the same unprotected operation.

Reviewed production source snapshot: HEAD `6a08069a121d81e85feb2199e39f2fe76bf9197c` plus concurrent OOM-owner worktree edits, UTC `2026-11-02T06:23:40.759195+00:00`. Per-file SHA-256, current signatures/body ranges, exact growth expressions, archived analyzer origin/call chain, local catches and outer-boundary reasoning are captured for all **259** formerly unreviewed production emissions in [exception-source-evidence.tsv](exception-source-evidence.tsv). Changed source lines are not fresh analyzer diagnostics.

The full **307-row** [exception-boundaries.tsv](exception-boundaries.tsv) and **3717-row** master classification retain every archived warning. The other **21** previously delegated rows are now individually classified: 11 output-flush performance advisories, 7 intentional glibc interposition ABI names and 3 intentionally discarded allocation-test results. They are not exception escapes.

## Shared causes, exact-site grouping

Every group below has exact source/archived line/symbol/current expression membership in the source-evidence TSV, not a category-wide assertion. “Other” distinguishes bounded output, proved guards and changed signatures.

| Exact-source family | Emissions | Source-confirmed barriers | Other reviewed dispositions |
|---|---:|---:|---:|
| E01 backend demux/decode/content | 13 | 13 | 0 |
| E02 subtitle/cover text ownership | 14 | 12 | 2 |
| E03 backend encoder ownership/packets | 18 | 18 | 0 |
| E04 muxer attachments/queue/header | 9 | 9 | 0 |
| E05 worker/pipe/pumper | 20 | 16 | 4 |
| E06 coordinator config/tag helpers | 2 | 2 | 0 |
| E07 zimg scratch/slot ownership | 4 | 4 | 0 |
| E08 FFmpeg encoder errors/metadata | 10 | 10 | 0 |
| E09 FFmpeg wrapper owner creation | 7 | 7 | 0 |
| E10 video-input error ownership | 4 | 4 | 0 |
| E11 logging/OCR/route | 5 | 5 | 0 |
| E12 public stage/wiring | 24 | 22 | 2 |
| E13 analytics lane/state/diagnostics | 19 | 16 | 3 |
| E14 audio filter graph/chain | 31 | 25 | 6 |
| E15 filter worker/queue/save | 9 | 9 | 0 |
| E16 video filter graph/state/diagnostics | 62 | 59 | 3 |
| E17 public Transcoder ownership/config | 8 | 7 | 1 |

### Why the outer catch does not close these sites

- `83e31c4e` coordinator `Run` catches only exceptions that can reach it. C++ terminates when an exception escapes an intervening noexcept helper.
- `Safe::String::ReplaceToken(...)`/string concatenation is evaluated **before** calling `Fail`/`Log`. An emergency catch inside `Fail` does not cover construction of its argument; a logger being disabled does not skip argument evaluation.
- Dynamic packet/sample/slot/strip queues, maps, scratch vectors, `MakePointer`/`MakeUnique`, config-map copies and exception-message owners have no universal no-growth invariant. Each TSV row identifies its own operation.
- `Decoder::SetupPlan` and demux-to-decoder `operator>>` have catches, but format their diagnostics **inside the catch handlers**, unprotected by another catch. `AVEncoder::Open` catches `PackedKeys` only. OCR `Engine::Recognize` catches image-padding growth only, then allocates exception owners/results outside that protection.
- Removing an outer reports wrapper's noexcept (`c751a034`) cannot fix a still-noexcept override or an inner Base constructor. Current override propagation changes are recorded as worktree changes, not committed end-to-end fixes.

## Guarded reports versus independent allocations

- `AfterCommas`: loop `i<fields.size()` proves `substr(i+1)` position <=size. View-only result; no owning allocation in the body.
- `NewlinesFromAss`: loop `i+1<text.size()` proves the erase position valid. Erase shrinks and the return moves the parameter. Any copying of the by-value input belongs to the caller.
- `SplitMime`: slash checks prove view substr bounds; **separate** `Lower`/string assignments allocate variable MIME names under noexcept.
- `ConfigureEncoder`/`StampMuxTags`: optional presence checks prove the reported `.value()` accesses valid; **separate** by-value config strings/maps and map-inserting tag setters allocate.
- `Muxer::Disposition`: contains-before-at proves the reported key-bound error unreachable absent unsupported concurrent mutation. Its return still constructs an owning `Safe::Optional<Disposition>`. Base `optional.txx` constructors call `Make`; `optional.hxx:770–776` calls `Heap::Allocate(sizeof(T))` even for scalar payloads.
- Public `Transcoder` constructor guards `duration.value()`. Its **independent** backend `MakePointer` initializer remains an allocation barrier in the reviewed snapshot.
- `AVFormatContext::HarvestSideData` and `Mp4ToAnnexB` guard expected values but still create shared owners/decoder containers under noexcept.

These are not blanket false positives and not repetitions of the same invalid `.value()` claim.

## Thirteen bounded-output sites: no invented confirmed bug

Base `Safe::String::SSO_CAPACITY=32`; `String::ReplaceTokens` (`../base/lib/public/StormByte/safe/string.cxx:550–555`) writes inline for output<=32. `ReplaceToken` uses temporary std::strings and numeric `std::to_string` (`safe/string.hxx:633–661`, `safe/replace_token.hxx:319–338`); their standard contracts remain potentially throwing. Finite output alone is **not** proof every current libc++ formatting leaf is allocation-free. These exact sites are retained as bounded implementation/portability contract risks, not declared runtime failures or categorically false positive. This is the concrete remaining leaf question, rather than 259 unreviewed symbols.

| Source | Symbol | Bound/provenance |
|---|---|---|
| `lib/private/StormByte/multimedia/backend/pipeline/detail/worker/decode.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/private/StormByte/multimedia/backend/pipeline/detail/worker/encode.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/private/StormByte/multimedia/backend/pipeline/detail/worker/mux.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/private/StormByte/multimedia/backend/pipeline/detail/worker/remux.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/public/StormByte/multimedia/pipeline/demuxer.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/public/StormByte/multimedia/pipeline/encoder.cxx` | `Ns` | Ns: signed int64 nanoseconds -> at most 20 decimal bytes, or empty view. |
| `lib/public/StormByte/multimedia/pipeline/filters/audio/adeclick.cxx` | `Chain` | Threshold clamped to [1,100]; default six-decimal conversion <=10 bytes; adeclick=threshold= prefix18 -> output<=28. |
| `lib/public/StormByte/multimedia/pipeline/filters/audio/downmix.cxx` | `AformatChain` | LayoutName switch returns fixed views, longest octagonal (9); aformat=channel_layouts= prefix23 -> output<=32. |
| `lib/public/StormByte/multimedia/pipeline/filters/audio/resample.cxx` | `Chain` | Rate returns int; signed decimal <=11 bytes; aresample=osr= prefix14 -> output<=25. |
| `lib/public/StormByte/multimedia/pipeline/filters/audio/upmix.cxx` | `AformatChain` | LayoutName switch returns fixed views, longest octagonal (9); aformat=channel_layouts= prefix23 -> output<=32. |
| `lib/public/StormByte/multimedia/pipeline/filters/video/cas.cxx` | `Chain` | Strength clamped to [0,1]; default six-decimal conversion <=8 bytes; cas=strength= prefix13 -> output<=21. |
| `lib/public/StormByte/multimedia/pipeline/filters/video/scale.cxx` | `Setup` | uint32 width/height -> <=10 decimal bytes each; target WxH output<=29. |
| `lib/public/StormByte/multimedia/pipeline/transcoder.cxx` | `KindName` | ToString(Type) switch has longest literal Attachment (10), including Unknown/Invalid fallback. |

`Ns` and `KindName` also return std::string; 20/10-byte bounds fit the usual current libc++ short-string representation, but the report does not promise that representation on every supported implementation.

## Worktree fixes and newly reproduced Base blockers

The OOM owner has changed the six original sites: AVSubtitle constructor/FillText now use `new(std::nothrow)` with null handling; Step/FFmpeg constructor chains propagate; CountFrames/VMAF reports propagate. It also changed PSNR/SSIM/XPSNR/Denoise/Loudnorm/Degrain Report overrides (six archived emissions formerly in the 259). Source status is **uncommitted/pending validation** at this review; no production edits were made by this worker.

Owner reports **26 production syntax checks PASS** and **subtitle fault injector PASS**. Its real factory/report injectors now reproduce **two distinct Base local barriers**:

1. `Telemetry::Telemetry noexcept` -> store `MakeUnique` (`stage-constructor-backtrace.log`).
2. `Safe::Map::Map() noexcept` -> `CreateHeader` (`report-failure-backtrace.log`).

Those are dependency-owned, separate from the earlier Base unwind-instrumentation fix. Proposed dependency work is still owner/parent-coordinated; propagation at Multimedia constructors/report wrappers is not an end-to-end pass while these barriers remain. The complete additional local-symbol evidence was sent directly to OOM owner `3dab7e25-b843-4ccc-9378-d5b2285d3967`, including the bounded-output distinction.

## Committed boundaries and validation that remain valid

- Multimedia **83e31c4e**: coordinator startup/run allocation handling, failure/cancel fallback and unwind-safe summary teardown.
- Multimedia **23b20736**: actual dependency reader/writer teardown on the exhausted coordinator.
- Base **ce1baac254e8cb20055398c1302d28930b445229**: no telemetry allocation during stack unwinding.
- Historical checkpoint commits: `25b1aa6a` Registry/catalog; `fb15e198` telemetry labels/snapshots; `ff525ebb` File probing/duration; `b23b01a8` Plan; `c751a034` outer reports/error snapshots; `01af1f94` emergency failure/closure/NLMeans. Exact current signatures take precedence over commit titles.
- Ten archived mappings remain exact: private coordinator ErrorText, ~TelemetrySummary, Start, Run; public Transcoder destructor, Fail, DurationProgress, OnSettled; FFmpeg::Fail; NLMeans::Process. See TSV for per-site rationale. These treatments are not fresh analyzer-clean assertions.
- Recorded evidence under `doc/logs/dependencies/oom/`: isolated UBSan **9 PASS** including real dependency teardown; strict ASan Telemetry **PASS**, allocator interposition **SKIP77**. Parent reports shared graph build success via `/fast` after unrelated generated-pkg-config reinstall collision, focused **4 PASS / 6 intentional ASan interposition SKIP, rc0**. Those results precede the newly identified constructor/report dependency blockers.

## Tests and verification limits

The 32 test exception emissions are explicitly split into **25 ordinary potentially-throwing main entries**, **1 guarded duration-observer access**, and **6 test-only noexcept capture/model callbacks**. Test entry failure on uncaught fixture allocation is not a production boundary defect. Growing capture vectors/frame copies/long failure diagnostics under noexcept remain test-only allocation-contract concerns; they are not falsely called fixed.

No fresh full clang-tidy, broad build/test, build-graph change, production edit or shared CHANGELOG edit was performed by this triage. Only artifact consistency checks were run. The accepted 75-emission numeric closure is unchanged. OOM owner/parent retain source fixes, dependency coordination and runtime validation.
