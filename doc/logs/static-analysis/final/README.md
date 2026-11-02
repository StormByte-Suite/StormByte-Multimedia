# Static-analysis triage and exception-boundary reconciliation

## Scope and evidence

- Repository snapshot at triage: `4aad8cae2e7aa4be82fdba7a6b757bde0b21bf66` (workspace has unrelated/in-flight modifications; production sources and CHANGELOG were not changed).
- Archived source of truth: `doc/logs/static-analysis/current/clang-tidy-diagnostics.txt`; full run: `doc/logs/static-analysis/current/clang-tidy-current.log`.
- The archived run used clang-tidy 22, 186 compilation-database translation units, and checks `clang-analyzer-*`, `bugprone-*`, `performance-*`, `portability-*`; run command and output are preserved in the full log.
- Diagnostics file SHA-256: `d57941308d1e4a7741ade3cb2961199701a1bfb5f27dab2d130efbd917f82113`. It contains 3717 warning emissions, represented in the TSV by 3717 distinct source/line/column/check/message rows with repeated emissions counted explicitly.
- Prior history reviewed: session checkpoints 001–013. These establish earlier targeted reviews (including `AVPointer`/Frame/Packet move semantics, `Plan::Policy` bitmask use, intentional `#pragma once`, optional guards, rounding, and virtual cleanup calls).
- The diagnostics are a 2026-11-02 06:08 snapshot. They are not a fresh run against the current dirty workspace; warnings at changed sources remain subject to the source-snapshot caveat below.

## Triage result

**Superseding OOM result:** the 238 barrier emissions and thirteen bounded
formatting risks below are the historical inventory. All 251 now have exact
current treatments in [the final batch](oom-batch/README.md). The twelve
constructor/report/subtitle changes are committed and validated. Final
allocator injection is 12 PASS; shared strict validation is 17 PASS / 12
intentional SKIP, with an overlapping final delta of 9 PASS. Residual semantic
size-limit warnings are explicitly retained, not declared false positives.
The counts/table below describe the earlier triage snapshot; the TSV
dispositions and final-batch records supersede its open OOM status.

**Confirmed residual non-allocation/non-noexcept defects: 0.** No report was promoted to a bug solely from a static warning. Follow-up review traced all **75 numeric emissions / 52 normalized sites** to actual bounds or intended representation contracts; see [numeric evidence](numeric-evidence.md). None remains unreviewed. The 143 adjacent-parameter API-design warnings remain unconfirmed rather than mislabeled false positives.

**OOM reconciliation is not an all-clear:** all 307 exception/new emissions now have concrete contract dispositions, with **238 source-confirmed surviving local noexcept barriers**, **12 worktree fixes pending completion/validation**, 10 mapped committed boundaries, 13 bounded-output implementation/portability risks, 2 proved guarded bodies and 32 classified test/harness emissions. These are grouped shared causes, not 238 independent runtime-reproduced bugs. All 259 formerly unreconciled production emissions were source/callee reviewed; no generic unreviewed-production bucket remains. See [exception-boundaries.md](exception-boundaries.md), its exact-symbol TSV and [current-source/analyzer proof](exception-source-evidence.tsv). Two newly reproduced Base constructor barriers still block the pending factory/report fixes; the earlier shared-graph PASS is not an all-clear.

| Classification | Emissions | Interpretation |
|---|---:|---|
| `SOURCE_CONFIRMED_LOCAL_NOEXCEPT_BARRIER` | 238 | Per-symbol throwing allocation/growth outside an encompassing catch; grouped shared causes, source proof only. |
| `WORKTREE_ALLOCATION_BOUNDARY_PENDING_VALIDATION` | 12 | Source signature/nothrow allocation changed; uncommitted and dependency validation pending. |
| `COMMITTED_ALLOCATION_BOUNDARY` | 10 | Ten exact archived sites map to committed local catches/propagation changes. |
| `REVIEWED_BOUNDED_FORMATTING_CONTRACT` | 13 | Exact output bounds reviewed; stdlib formatting leaf allocation/portability not assumed safe or called a confirmed runtime bug. |
| `PROVED_GUARDED_NONALLOCATING_BODY` | 2 | AfterCommas view substr and NewlinesFromAss shrinking erase have proved bounds. |
| `REVIEWED_TEST_ENTRY_EXCEPTION_POLICY` | 25 | Ordinary potentially-throwing test main; no production noexcept promise. |
| `REVIEWED_TEST_CALLBACK_GUARD` | 1 | Duration test optional access guarded by presence. |
| `TEST_ONLY_NOEXCEPT_ALLOCATION_CONTRACT` | 6 | Growing capture/model callback allocations under test-only noexcept. |
| `REVIEWED_GLIBC_INTERPOSITION_ABI` | 7 | Reserved identifiers intentionally name external glibc allocation ABI in isolated regressions. |
| `PERFORMANCE_ADVISORY` | 2652 | Optimization hint only; no correctness defect established. |
| `GENERATED_ARTIFACT` | 287 | Generated unity aggregation path outside the repository; not a checked-in source defect. |
| `OPEN_API_RISK` | 143 | Potential API usability issue; no concrete swapped invocation was established, so retain for targeted contract review. |
| `PROJECT_STYLE_POLICY` | 69 | Intentional project convention required by CODING_STYLE.md. |
| `REVIEWED_MOVE_SEQUENCE` | 51 | Move sequences transfer independent state and/or establish the documented empty moved-from state. |
| `PROVED_FIXED_FIXTURE_RANGE` | 26 | AVI iterator offsets, memory-writer offsets, and TokenReader lengths are bounded by enumerated fixed test inputs. |
| `PROVED_FIXED_FRAME_RANGE` | 25 | Explicit CPU fixture shapes/formats and allocator-derived strides bound every flagged row/component product. |
| `PROVED_CONSTANT_RANGE` | 12 | Literal products are below target integer limits before widening; exact values are tabulated in numeric evidence. |
| `REVIEWED_SECONDS_REPRESENTATION` | 3 | Timestamp conversion intentionally uses double seconds; low-tick precision loss above 2^53 is documented, not claimed absent. |
| `PROVED_CONSTANT_AND_RING_RANGE` | 2 | Degrain reserve uses fixed constants; candidate count is at most 10 before conversion to double. |
| `PROVED_LOOP_COUNTER_RANGE` | 2 | PTS counter products have explicit maxima 10,010 and 7,168. |
| `PROVED_SIGNED_DIMENSION_PROVENANCE` | 2 | Parsed fixture dimensions originate in signed FFmpeg int fields, not arbitrary uint32_t Resolution inputs. |
| `PROVED_AUDIO_PRODUCER_RANGE` | 2 | Producer rejects audio above 64 MiB; enumerated WAV generator callers remain below uint32_t multiplication limits. |
| `PROVED_INPUT_RANGE` | 1 | Palette index originates in uint8_t, so multiplying it by four is at most 1,020. |
| `REVIEWED_CONCRETE_CLEANUP` | 26 | Intentional concrete cleanup dispatch; no invalid constructor/destructor virtual dispatch found. |
| `REVIEWED_BITMASK_ENCODING` | 25 | Plan flags intentionally use uint32_t bitmask values including reserved resource bits. |
| `TEST_DIAGNOSTIC_LABEL` | 21 | Only changes test label identity, not behavior under test. |
| `REVIEWED_OPAQUE_C_API_POINTER` | 13 | Pointer indirection matches the opaque C API object/state contract. |
| `REVIEWED_GUARDED_OPTIONAL` | 9 | Control-flow/paired assignment establishes the optional before access. |
| `REVIEWED_NONNEGATIVE_ROUNDING` | 5 | Positive-only sample/weight data makes +0.5 rounding intentional. |
| `REVIEWED_INITIALIZE_BEFORE_USE` | 4 | C struct defaults are replaced before library consumption; failed initialization returns early. |
| `REVIEWED_BEST_EFFORT_CLEANUP` | 3 | Exception suppression is contained to fallback logging or noexcept cleanup/destruction. |
| `REVIEWED_C_API_STATUS_ASSIGNMENT` | 3 | Return-code assignment is used intentionally as the FFmpeg API condition. |
| `REVIEWED_INTENTIONAL_RESULT_USE` | 6 | Return values are explicitly discarded for side effects, ownership transfer, or exception-only test execution. |
| `REVIEWED_EQUIVALENT_BRANCHES` | 2 | Same operation is required for separate enum/fallback cases; no semantic mismatch shown. |
| `REVIEWED_STABLE_PRIORITY_ORDER` | 2 | Comparator sorts by explicit preference, not pointer address. |
| `REVIEWED_DEFAULT_OVERWRITTEN` | 2 | Initial values are replaced in every branch before use. |
| `REVIEWED_ALIGNMENT_IDIOM` | 1 | Intentional 64-byte interior alignment within an overallocated scratch buffer. |
| `REVIEWED_REBUILT_BASE` | 1 | Copy constructor rebuilds the base through add(); direct base copy would duplicate elements. |
| `REVIEWED_MOVED_FROM_TEST` | 1 | Test intentionally checks the documented empty moved-from state. |
| `BOUNDED_TEST_ARITHMETIC` | 1 | Small explicit test dimensions bound the arithmetic; related production fix is committed. |
| `REVIEWED_NULL_TERMINATED_PROVENANCE` | 1 | The string_view is sourced from a process-lifetime FFmpeg C string. |

## Reconciled findings and fixes

- The previously noted NLMeans overflow/sample-range candidate is resolved in committed `1faf07d5` (`Fix NLMeans overflow and sample range on 16-bit frames`). Current `nlmeans_plane.hxx` widens the signed difference before squaring; the 16-bit tests exercise that path. The remaining `+0.5` diagnostics are intentional nonnegative rounding, not the earlier overflow.
- Degrain extreme chroma-coordinate overflow is addressed in `68777c7b`; current coordinates widen before multiplication. Subtitle bitmap stride/stacked-geometry and signed video-row/sample offsets are addressed in `9d8fc17e` and `a773484f`. These commits are reconciliation evidence, not new fixes from this triage.
- `bugprone-use-after-move` is not accepted as a blanket false positive: individual wrapper/frame/packet sequences were checked. Base pointer movement does not invalidate independent fields; Frame/Packet call `BecomeEmpty()` after transferring members; the DOVI test asserts that documented state.
- Optional and enum warnings were checked at use sites: SimplePlayer emplaces the scaler and creates/ensures the tone-map graph before dereference; VMAF stores `mean`/`min` together; HDR10+ test assertions guard PTS. `Plan::Policy` is a `uint32_t` bitmask with reserved resource bits 29/30, not a closed enum.
- 287 suspicious-include emissions refer only to generated `/tmp/BMDATA/plain-multimedia/lib/tables/tables_unity.cxx`; the generator aggregates source includes. No checked-in include line is represented by those diagnostics.
- 2,546 `performance-avoid-endl` emissions are flush/performance advice, not correctness failures. Other performance checks are similarly kept as optimization advice rather than “false positives.”
- Numeric follow-up: all 43 multiplication-widening and 32 narrowing emissions were traced to their actual inputs, loops, allocator/metadata provenance, or floating seconds representation. [numeric-evidence.md](numeric-evidence.md) groups all 52 normalized sites into 15 explicit contracts, including repeated header emissions and archived/current line shifts. No numeric candidate remains generically labeled open.

## Delegated/open and snapshot caveats

- All 307 exception/new emissions now have exact source/callee-contract dispositions: 238 source-confirmed barriers, 12 pending worktree fixes, 10 committed treatments, 13 bounded-output risks, 2 proved guards and 32 classified test emissions. None of the formerly unreviewed 259 production emissions remains in a generic unresolved bucket.
- Multimedia `83e31c4e` + `23b20736` and Base `ce1baac` are committed. Recorded isolated UBSan real-teardown validation is 9 PASS; strict ASan Telemetry PASS / interposition SKIP77. Parent reports the shared graph built and its focused selection returned 4 PASS / 6 intentional ASan interposition SKIP, rc0. That result predates the residual-barrier edits now owned by the OOM worker.
- The archived run is from 06:08, before those commits. The TSV's source excerpts/worktree-state column were captured during earlier triage and are not fresh post-commit diagnostics. Named expressions, commit references, and the exception/numeric evidence resolve changed line numbers. No full build graph/configuration or test suite was run by this triage.
- `bugprone-easily-swappable-parameters` remains an API-design signal without an actual swapped call; its 143 exact locations remain in TSV. Numeric candidates are now reconciled with explicit bounds/provenance, rather than left open or dismissed by category.

## Files

### Superseding OOM validation update

The earlier 07:04 Buffer-writer teardown finding is addressed by Base `ce1baac`, and Multimedia `23b20736` now tests **real** dependency-owner teardown on the exhausted thread. The recorded result is 9 PASS isolated UBSan, with Base strict ASan Telemetry PASS and allocator interposition SKIP77. See `doc/logs/dependencies/oom/base-fix-validation.txt`. Parent reports shared-graph build success and 4 PASS / 6 intentional ASan interposition SKIP, rc0. This success does not resolve the unrelated local `noexcept` barriers listed in the exception reconciliation. The original six-site fixes and six more report overrides now propagate/use nothrow allocation in the owner's worktree, pending commit/validation. Real factory/report injection exposed Base Telemetry and Safe::Map constructor noexcept allocations; those are not fixed by the earlier Base unwind guard. All additional exact residual sites and bounded-output caveats were sent directly to the OOM owner.

- Full line-level classification: `doc/logs/static-analysis/final/clang-tidy-triage.tsv`.
- Numeric bounds/provenance closure: `doc/logs/static-analysis/final/numeric-evidence.md`.
- Current source/callee proof: `doc/logs/static-analysis/final/exception-source-evidence.tsv`.
- Exception-boundary mapping and residual symbols: `doc/logs/static-analysis/final/exception-boundaries.md` and `exception-boundaries.tsv`.
- Raw preserved findings: `doc/logs/static-analysis/current/clang-tidy-diagnostics.txt`.
- Full clang-tidy transcript: `doc/logs/static-analysis/current/clang-tidy-current.log`.
- This triage changed documentation only; no production code, shared CHANGELOG, releases, tags, or pushes were touched.
