# Contributing to StormByte-Multimedia

Issues and pull requests belong on this repository. Ask the maintainers which
branch should receive a change; do not assume a development branch is permanent.

Read [CODING_STYLE.md](CODING_STYLE.md) before preparing a patch. It describes the
project's C++26 conventions, formatting, ownership, visibility, documentation,
and validation requirements.

## Contribution Rights

By submitting a contribution you assign copyright in that contribution to the
copyright holder of this repository, David C. Manuelda. The dual license in
[LICENSE](LICENSE) can then apply to it.

Submit only material you wrote and are free to assign. Do not submit material
owned by an employer, a third party, or another project unless you have the right
to assign that copyright here. Each contributor is responsible for obtaining
that clearance; the project does not audit origin or assume that liability.

If material was submitted without those rights, the repository copyright holder
may revert the associated changes. The true rights holder may also request their
removal directly.

Preserve the repository's existing license notices. CMake and Markdown files do
not use the C++ source banner. Third-party code retains its own licenses and must
not be presented as original StormByte-Multimedia work.

## Issues

Include the platform, compiler and standard library, relevant dependency versions,
build configuration, and steps to reproduce. For media failures, include a minimal
sample that you are permitted to share, or instructions for generating one.

Describe expected and actual behavior. Provide relevant logs and telemetry rather
than only a screenshot of the final error. Remove credentials, personal metadata,
and private paths before posting diagnostics.

## Pull Requests

- Keep the diff focused; avoid unrelated renames, formatting, or dependency changes.
- Preserve public API and cross-platform behavior unless the change explicitly requires otherwise.
- Add or extend regression tests for behavioral changes.
- Include EOF and flush behavior, cancellation, and concurrency cases where relevant.
- Preserve stream metadata and distinguish remuxing from decoding and encoding.
- Do not modify vendored sources in a feature patch; discuss dependency work separately.
- Update user-facing documentation when behavior, API, or build options change.
- Explain validation performed and identify unavailable platforms or fixtures.
- Maintainers may request a rebase onto the current integration branch.

## Building and Documentation

See [README.md](README.md) for build prerequisites, dependency options, and license
implications of FFmpeg features. Keep build outputs out of source directories and
use separate build trees for compiler or sanitizer configurations.

Enable documentation with `ENABLE_DOC=ON`. Doxygen needs access to the suite's
HTTPS tagfiles. Include the generated filter header, but not dependency sources
or private implementation pages. Verify the resulting documentation without
warnings, including the main page, API links, and these contribution guides.

The `ENABLE_TEST` option enables test registration; it does not by itself provide
media fixtures or guarantee that every processing path has an executable test.
Describe any separate test programs and fixtures used to validate the patch.