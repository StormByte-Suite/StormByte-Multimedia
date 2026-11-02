# Allocation-boundary closure

This supersedes the earlier snapshot's 238 surviving local noexcept barriers
and thirteen bounded-formatting contract risks. All **251 exact archived
emissions** have a current treatment: **115 filter**, **78 backend** and
**58 wrapper/public-pipeline** rows. These counts describe warnings/sites,
not independent runtime-reproduced failures.

| Exact-site record | Scope |
|---|---|
| [Filter reconciliation](filters-exception-site-reconciliation.tsv) | Local catches at lifecycle boundaries; allocating helpers and constructors propagate |
| [Backend reconciliation](backend-exception-sites.tsv) | Worker/pumper failure boundaries, propagation and allocation-free cleanup |
| [Parent reconciliation](parent-exception-site-reconciliation.tsv) | FFmpeg/OCR owners, configuration, graph wiring and public pipeline |
| [Filter warning triage](filters-warning-triage.tsv) | Exact residual call chains, bounds and retained semantic risks |
| [Backend warning triage](backend-warning-triage.tsv) | Exact residual call chains and source fixes |

The original constructor/report/subtitle twelve-site batch is separately
committed and validated in [local-barriers](../../../dependencies/oom/local-barriers/README.md).
The main historical tables preserve archived source excerpts and hashes; their
updated dispositions refer to these superseding records, not newly hashed
historical source.

## Validation

Both shared and isolated builds passed. Final isolated UBSan allocator injection
is **12 PASS**. Shared strict ASan/UBSan/LSan is **17 PASS / 12 intentional
allocator-interposition SKIP**, followed by **9 overlapping PASS** after the
last fixes. See [commands, logs and limits](../../../dependencies/oom/final-batch/README.md).

The final clang-tidy run covers **90 changed translation units**, using the
combined build's compilation database. All analyzer invocations compiled and
returned zero. [results.json](results.json) and each per-unit log preserve
the warnings; **exit zero is not warning-free**.

The scope is allocation exception safety. Catches name `Safe::AllocationError`
and `std::bad_alloc`; they do not swallow every exception. Nested logger
barriers were removed or guarded. Failure paths use message-free owner failure,
and teardown must still close/wake/join even when logging allocation fails.
Ownership guards release encoder contexts, dictionaries, graph contexts,
metadata and OCR text during unwind.

Code is committed in `88b9f418`, `3a8a80b6`, `a46701f6`, `08a0461d` and
`a354f199`. Each includes a changelog entry; validation ran on their combined
source state before splitting the commits.

## Residual warnings: explicit non-allocation limits

The final logs still include standard `length_error` and `out_of_range` paths.
The agent ledgers are exact-chain reviews from the preceding pass; fixed rows
are historical and may be absent from the final logs. Newly exposed paths in
the same functions remain visible in the final logs.

For the parent-owned functions:

- Decoder setup/stamping/reset/look diagnostics and Muxer wiring reach
  `PadAscii::insert`. Its insertion position is zero, or one only for a
  nonempty signed string: position cannot exceed size. Step work diagnostics
  use the same guarded numeric formatting. Step wiring's string prefix inserts
  at zero, also valid.
- Demuxer/Encoder `Ns` render signed int64 nanoseconds (at most twenty
  characters), or a fixed absence marker. Track numbers, resource counters
  and track-kind names have fixed numeric/enumerator bounds.
- Copying Step's downstream vector copies an existing vector of the identical
  pointer type and allocator, so its size is already representable.
- Dynamic graph growth, caller-derived names/paths/error strings, and copying
  differently represented strings retain **semantic maximum-size risks**.
  Examples include Step connection capacity, Filters' measurement list,
  Route filter-name diagnostics, Transcoder source-path/settled-track
  diagnostics and Muxer output diagnostics. These are not proved impossible
  and are not mislabeled false positives.
- The filter ledger explicitly retains oversized arbitrary VMAF model names
  and FFmpeg filter names. No realizable triggering input or runtime failure
  was established for those enormous-length paths; this batch does not claim
  to make every standard-library exception safe under all possible inputs.

All identified ordinary allocation barriers in the archived inventory have
treatments. This is not a universal exception-safety proof or a claim that the
entire audit is warning-free. The 143 adjacent-parameter API advisories remain
unconfirmed API risks, not confirmed bugs. NVIDIA's proprietary-driver exception
remains separately documented and user-authorized.
