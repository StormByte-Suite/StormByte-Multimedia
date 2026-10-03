# StormByte-Multimedia Coding Style

Multimedia follows the StormByte suite conventions. Match nearby code when a
detail is not specified here, and keep changes focused on the requested behavior.

## Files and Formatting

- Use `.hxx` for headers, `.cxx` for sources, and `.txx` for template bodies.
- Headers use `#pragma once`. Preserve the repository's existing license notices.
- Indent with literal tabs, not spaces. Do not run clang-format on this project.
- Use K&R braces, with access labels one indentation level inside the class.
- Put `else` on its own line. Single-statement branches normally omit braces.
- Bind pointers and references to the type: `Frame* frame`, `const Packet& packet`.
- Use PascalCase for types and functions, and uppercase underscore-separated macros.
- Keep one statement per line and end every file with a newline.

Include StormByte headers first, followed by a blank line and standard-library
headers. Keep each group alphabetical. Never use `using namespace` in headers;
namespace directives after the includes are appropriate in source files.

Use nested namespace blocks in headers up to three levels deep. Deeper namespaces
may use qualified namespace syntax. Document each namespace consistently.

## Language and Ownership

Use C++26 and RAII. Prefer existing StormByte concepts, traits, buffers, safe
pointers, strings, and `Expected`/`Unexpected` helpers when they fit the contract.
Use domain exception types rather than introducing unrelated standard exceptions.

Prefer scoped enumerations and explicit converting constructors unless an implicit
conversion is intentional and documented. Use `constexpr` where appropriate and
`noexcept` only when the implementation honors that contract. Do not let exceptions
escape FFmpeg callbacks or worker entry points.

Preserve the suite's platform and visibility macros. Changes must support Linux,
Windows, and macOS, or document a deliberate best-effort capability on unsupported
platforms. Avoid assumptions about the size of `long`, character encodings, or
shared-library allocation ownership.

## Public API and Library Boundaries

Preserve existing public signatures unless an API change is explicitly intended.
Use `STORMBYTE_MULTIMEDIA_PUBLIC` on exported classes and declarations and
`STORMBYTE_MULTIMEDIA_PRIVATE` on implementation types. Visibility attributes on
free functions precede the return type. Do not repeat a class export attribute on
its members or on ordinary source definitions.

Use the repository's established boundary-safe value types for new interfaces.
Move heap-affecting operations out of headers when required for shared-library
ownership. Ordinary `inline` is not a guarantee that allocation happens in the
caller's runtime. Preserve the documented lifetime of references and shared handles.

Keep third-party handles behind the established multimedia abstractions. Do not
add implementation details to an interface merely to support one caller.

## Documentation

Document declarations in headers, including constructor overloads, parameters,
return values, ownership, failure behavior, and private implementation members.
Use `@ref` only for actual documented symbols, with qualified names when ambiguous.
Template examples belong in code spans or code blocks, not unescaped HTML tags.

Published API documentation describes what callers can use and observe. It must
not explain private fields, queues, backend classes, or helper call sequences.
Keep implementation documentation separate or inside Doxygen internal sections;
private extraction and internal documentation remain disabled for publication.

Resolve other suite modules through Doxygen tagfiles rather than adding their
sources to the documentation inputs. Generated documentation must build without
warnings. Update the README when public behavior, configuration, or usage changes.

## Validation and Commits

Add focused regression coverage for changed behavior, including relevant EOF,
flush, cancellation, concurrency, and platform cases. Verify shared-library
compatibility and preserve media metadata when changing processing paths.

Use separate build directories for different compilers and configurations. Check
warnings with `-Wall -Wextra -Wpedantic -Werror` where supported. Report what was
actually executed and any validation limitations.

Use Conventional Commits in English, with a concise subject and a body explaining
motivation, behavior, compatibility, and validation. Keep each commit on one topic.