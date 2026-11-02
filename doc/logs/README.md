# Audit evidence

This directory preserves the available evidence for the static/sanitizer audit
and the upstream fixes described in [PR-DRAFTS.md](../PR-DRAFTS.md).
It is a snapshot, **not a passing release gate**.

## Start here

| Area | Contents |
|---|---|
| [Current results](runs/current/) | Complete strict suite and fourteen-provider batch, both log and JUnit XML |
| [Clean build](builds/clean-final/) | Configure/build from a new directory; initial 21-fix state |
| [Follow-up builds](builds/) | Subsequent configure/build logs through the current 64-fix state |
| [Test progression](runs/) | Original baseline, provider batches and narrowed follow-ups |
| [Provider evidence](providers/) | Subfolders for all ten upstream repositories, with standalone sources where available and indexed CTest output |
| [Static analysis](static-analysis/) | Current clang-tidy output plus initial per-unit diagnostics and historical triage |
| [Dependency tests](dependencies/) | Available Base/Buffer/Logger/System CTest records |
| [Reproduction commands](reproduce/) | Build/test recipe and isolated-main runner |
| [Historical recovery](historical/) | Earlier UBSan/recover-mode runs and probes, not current strict validation |
| [Snapshot metadata](snapshot.json) | Revisions, tool versions, dirty-file hashes and validation scope |
| [File inventory](inventory.tsv) | File categories, origin, size and SHA-256 |
| [Integrity checks](SHA256SUMS) | Checksums for every other evidence file |

### Results at this snapshot

- Latest consolidated state after test deadline/readiness fixes:
  **38 original failures now pass; 9 remain unresolved**. The remaining cases
  are the default live-audio-track cycling fixture and eight NVENC tests.
  [Per-case status](runs/followup/latest-original-failures.tsv) and
  [details](../PR-DRAFTS.md#subsequent-preview-and-duration-investigation).
  CUDA initialization fails in an instrumented standalone driver-only main
  but succeeds without instrumentation. A longer live-audio fixture passes
  all timing/epoch assertions; that does not erase the default-fixture failure.

Earlier focused iterations (retained for progression):

- Latest narrowed work closes all fifteen selected provider paths, including
  both real Dolby Vision RPU leaks, FDK stereo/surround, Vorbis decode and SVT
  cancellation/interleave. Seven Dolby Vision cases pass without suppressions;
  only NVIDIA `cuInit` is suppressed where needed in subsequent SVT runs.
  See [follow-up results](runs/followup/) and the
  [current PR summary](../PR-DRAFTS.md#validation-summary).
- Of the original 47 failed cases, 31 now have passing focused follow-up
  results; 16 remain unresolved. The latest other-failure batch is **3 passed,
  16 failed**, exit **8**. Serial preview validation is **0 passed, 7 failed**,
  exit **8**. No full-suite rerun was performed.
- Fifteen new mail patches apply on clean predecessor trees with exact
  resulting-tree comparisons; [verification](reproduce/git-am-followup-verification.log).
  There are now 64 fixes in [the patch map](reproduce/patches.tsv).

Original full-suite and provider-batch snapshot (unchanged):

- Current provider batch: **13 passed, 1 failed**, exit code **8**.
- Current complete suite: **377 passed, 47 failed, 11 skipped** out of 435,
  exit code **8**, 680.17 seconds.
- All four standalone reference checks pass with ASan/UBSan.
- Vorbis follow-up: the minimal residue ownership reproducer, all 144
  setup/clear configurations and the formerly failing surround encode pass
  with unsuppressed ASan/UBSan/LSan. See
  [Vorbis standalone evidence](providers/vorbis/standalone/).
- All twelve tests affected by proprietary NVIDIA `cuInit` leaks pass with the
  user-approved [narrow driver exception](reproduce/nvidia-driver.lsan),
  exit **0**. Leak detection remains enabled for all other allocations;
  [new results](runs/followup/nvidia-driver.xml) do not replace the original
  unsuppressed full-suite evidence or waive hardware capability failures.
- All 48 mail patches apply in filename order from their ten documented bases;
  resulting Git trees exactly match their authoring trees. See
  [mail-patch verification](reproduce/git-am-verification.log).

Full results include failures outside the fourteen provider paths. Read
[the complete test inventory](runs/current/test-results.tsv) and
[failed-test classification](runs/current/failures.tsv) rather than assuming
that a focused PASS covers the entire provider.

## Layout and chronology

`runs/baseline/` predates the upstream fixes and contains the original strict and
plain runs. `runs/initial-patches/` covers the first FFmpeg/provider batches.
`runs/clean-final/` is the first fresh-build batch (7/14). `runs/followup/` and
`runs/third/` reach 10/14. Later batches deliberately run only the remaining
failures, except `runs/seventh/` and `runs/current/`, which run all fourteen.
Do not compare a 1/2 or 0/1 narrowed batch with a fourteen-test total.

Build logs are retained verbatim. BuildMaster can execute dependency configure,
compile and install during top-level CMake configuration, so the top-level
configure log is also relevant build evidence. Sources can be restored by
BuildMaster after installation; source line numbers in old reports describe the
then-applied patches, not necessarily today's submodule worktree.

`static-analysis/initial/units/` contains the original numbered clang-tidy logs;
`results.json` associates each number with a translation unit.
The [historical audit driver](static-analysis/initial/static_audit.py) contains
machine-specific original paths and is preserved as provenance, not as a
portable runner. Warnings are candidates, not confirmed defects.

Earlier recover-mode/suppression experiments are explicitly under
`historical/`; their suppression file is evidence only. They are **not** the
options used by the current strict runs, and must not be used to call the
current suite clean.

## Reproducers, inputs and limitations

Use the [reproduction guide](reproduce/README.md) for executable commands.
Standalone programs live under their provider folders and include their own
`main`. Existing CTest reproducers remain in [the test tree](../../test/), with
their shared helpers and fixtures. They are indexed in
[ctest-reproducers.tsv](reproduce/ctest-reproducers.tsv); copying isolated
function bodies would omit required setup and is intentionally avoided.
Fixture provenance is documented in [test/files/README.md](../../test/files/README.md).

Provider `ctest-evidence.log` files contain exact output extracted from the
preserved JUnit XML, grouped by run and test. They include original reports,
subsequent different failures and passing output where available. Their
`ctest-index.tsv` files point back to the authoritative XML. Some old runs did
not produce XML; their complete text logs are still preserved under `runs/`.

The [patch index](reproduce/patches.tsv) lists all patches, bases, authoring
trees and provider evidence. Not every fix has a small standalone upstream-only
reproducer: most were discovered by existing multi-stage Multimedia tests.
Removal of a diagnostic is not proof of end-to-end success when a later
diagnostic stops that same test. No new SYSTEM-provider-sensitive CTest
requirements were introduced.

The original clean build reused locally prepared sanitizer-instrumented
StormByte ZIPs, not dependency objects or stamps. Binaries, build trees,
dependency ZIPs and media duplicates are intentionally excluded. The build
recipe requires equivalent instrumented dependencies; it must not silently
substitute published, non-instrumented ZIPs and claim equivalent coverage.

The validation snapshot included unfinished Transcoder OOM source changes.
Their exact diff is preserved in
[multimedia-transcoder-wip.patch](reproduce/multimedia-transcoder-wip.patch)
for reproducibility only. It is **not an approved fix** and is excluded from
the upstream patch series. Do not apply it to unrelated revisions.

Absolute build paths and original timestamps remain in raw logs for source
attribution. Relocation requires adapting the documented commands. The
preserved baseline/recovery records are historical and must not be mistaken for
the current checked-out revision.

## Integrity and updating

From this directory:

```sh
sha256sum --check SHA256SUMS
```

`inventory.tsv` records whether a file was copied byte-for-byte, extracted from
JUnit or generated during preservation. No missing historical shell output was
fabricated: standalone and complete-series application logs were captured by
rerunning those bounded checks. The main application suite was not rerun merely
to copy its already complete logs.
