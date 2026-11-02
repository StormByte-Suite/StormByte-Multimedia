# Proprietary NVIDIA driver exception

The user explicitly authorized ignoring leaks in the proprietary NVIDIA
driver, not leaks in FFmpeg, StormByte or open-source providers. This document
records the additional driver-only evidence; it does not introduce a blanket
LSan suppression or change test exit codes.

## Initialization and address-space mapping

The [driver-only initialization main](cuda-init-check.c) succeeds without
instrumentation and returns `CUDA_ERROR_OUT_OF_MEMORY` under default ASan.
Using `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:protect_shadow_gap=0`
allows it to initialize and to create/destroy a CUDA context, exit 0.
This option disables protection of ASan's address-space gap so the driver can
reserve its mappings; ASan's instrumented-access checks, UBSan and leak
detection remain enabled. This is a validation-environment adjustment, not
the default application configuration or a memory-error suppression.

See [initialization result](../runs/followup/cuda-init-shadow-gap.log) and
[context lifecycle result](../runs/followup/cuda-context-sanitized.log).

## NVENC session lifecycle

The [minimal encoder-session main](nvenc-session-check.c) links only `libdl`
and loads `libcuda.so.1` and `libnvidia-encode.so.1`. It initializes CUDA,
creates a context, opens an NVENC session, destroys the encoder, destroys the
context and unloads both driver libraries. It does not link FFmpeg or
StormByte. Its teardown reproduces **96 bytes in two allocations**, reported
through glibc's legacy `pthread_cond_broadcast` compatibility entry point.
That exact allocation signature appears after each NVENC session in the
Multimedia tests (96 bytes for one session, 192 for two).

[Minimal reproduction output](../runs/followup/nvenc-session-minimal.log):
LSan exit **134**, not a passing sanitizer run. Context creation/destruction
alone does not reproduce this leak. Both APIs' cleanup calls are present.
The existing `leak:cuInit` suppression does not match these allocations.
No suppression for `calloc`, `pthread_cond_broadcast` or all of libc was added:
it would hide unrelated application leaks.

```sh
clang-22 -Wall -Wextra -Werror -g \
  -fsanitize=address,undefined -shared-libasan \
  -I/tmp/BMDATA/sanitizer-clean-final/stormbyte-buildmaster/install/include \
  doc/logs/reproduce/nvenc-session-check.c -ldl -o /tmp/nvenc-session-check
LD_LIBRARY_PATH="$(clang-22 --print-runtime-dir):/usr/lib/llvm/22/lib64" \
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:protect_shadow_gap=0 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  LSAN_OPTIONS="suppressions=$PWD/doc/logs/reproduce/nvidia-driver.lsan:print_suppressions=1" \
  timeout 15 /tmp/nvenc-session-check
```

Full malloc-stack unwinding was attempted, stopped after excessive CPU
consumption, and then bounded to 15 seconds in the tiny reproducer. Neither
attempt produced useful deeper stacks. These are failed diagnostics, not
validation passes, and must not be repeated unbounded.

## Eight affected canonical tests

| Test | Functional result with CUDA-compatible ASan mapping |
|---|---|
| `pipeline.test_hardware_h264_nvenc_capability` | PASS, then driver-only LSan failure |
| `pipeline.test_hardware_h264_nvenc_10bit_capability` | SKIP: GPU lacks H.264 10-bit encoding, then driver-only LSan failure |
| `pipeline.test_hardware_hevc_nvenc_capability` | PASS, then driver-only LSan failure |
| `pipeline.test_transcoder_hevc_nvenc_encode_preserves_hdr10_metadata` | PASS, then driver-only LSan failure |
| `pipeline.test_nvenc_encodes_h264_mp4` | PASS, then driver-only LSan failure |
| `pipeline.test_nvenc_encodes_h264_10bit_matroska` | SKIP: GPU lacks H.264 10-bit encoding, then driver-only LSan failure |
| `pipeline.test_nvenc_encodes_hevc_main_matroska` | PASS, then driver-only LSan failure |
| `pipeline.test_nvenc_encodes_hevc_main10_matroska` | PASS, then driver-only LSan failure |

[Instrumented batch](../runs/followup/nvenc-shadow-gap.xml): CTest exit **8**,
eight process failures due to the isolated driver allocations. Functional
assertions complete before LSan teardown.

[Existing plain-build control](../runs/followup/nvenc-plain.xml): **6 passed,
2 skipped**, exit **0**. That pre-existing build predates the latest provider
and OOM edits; it is a hardware/control result, not certification of the final
source revision.

These eight sanitizer failures fall under the user's proprietary-driver leak
exception only because of the driver-only reproducer above. They are reported
as **external-exception**, never as eight clean sanitizer passes. Hardware
encoding failures, unsupported capabilities on other GPUs and any different
allocation signatures remain outside this exception.
