import json
import os
import shlex
import subprocess

root = os.getcwd()
out = root + "/build/oom-validation"
env = {**os.environ, "TMPDIR": out}
commands = json.load(open("build/ubsan/compile_commands.json"))
base = next(c for c in commands if c["file"].endswith("/backend/pipeline/transcoder.cxx"))
args = shlex.split(base["command"])
args = args[: args.index("-o")]
args[1:1] = [
    "-I" + root + "/../base/lib/public",
    "-I" + out + "/include",
    "-I" + root + "/build/sanitizer-validation/prefix/buffer/include",
]
args += ["-fno-crash-diagnostics"]
changed = subprocess.check_output(["git", "diff", "--name-only", "6a08069a", "--", "lib"], text=True).splitlines()
sources = [s for s in changed if s.startswith("lib/") and s.endswith(".cxx")]
for source in [
    "private/StormByte/multimedia/backend/pipeline/pipe.cxx",
    "private/StormByte/multimedia/backend/pipeline/worker.cxx",
    "public/StormByte/multimedia/pipeline/plan.cxx",
    "public/StormByte/multimedia/pipeline/filters/video/nlmeans.cxx",
    "private/StormByte/multimedia/backend/pipeline/transcoder.cxx",
]:
    if "lib/" + source not in sources:
        sources.append("lib/" + source)
lines = ["all: " + out + "/persistent-tests"]
replacements = {}
for source in sources:
    obj = out + "/current-" + source.replace("/", "_").replace(".cxx", "") + ".o"
    replacements["lib/CMakeFiles/StormByte-Multimedia.dir/" + source.removeprefix("lib/") + ".o"] = obj
    lines += [obj + ": " + source, "\t" + shlex.join(args + ["-c", root + "/" + source, "-o", obj])]
raw = subprocess.check_output(["ninja", "-C", "build/ubsan", "-t", "commands", "lib/libStormByte-Multimedia.so.1.0.1"], text=True).splitlines()[-1]
link = shlex.split(raw)
link = link[2 : link.index("&&", 2)]
i = link.index("-Xlinker")
del link[i : i + 2]
libs = link[next(i for i, s in enumerate(link) if s.startswith("-L")) :]
for i in range(len(link) - 1):
    if link[i] == "-o":
        link[i + 1] = out + "/libStormByte-Multimedia.so.1.0.1"
link = [replacements.get(s, s) for s in link]
lines += [out + "/libStormByte-Multimedia.so.1.0.1: " + " ".join(replacements.values()), "\tcd build/ubsan && " + shlex.join(link)]
test_args = args + ["-fvisibility=default", '-DSTORMBYTE_TEST_FILES_DIR="' + root + '/test/files"', '-DSTORMBYTE_TEST_OUTPUT_DIR="' + out + '/test-output"']
lines += [out + "/persistent.o: test/pipeline/filters/allocation_failure_test.cxx FORCE", "\t" + shlex.join(test_args + ["-c", root + "/test/pipeline/filters/allocation_failure_test.cxx", "-o", out + "/persistent.o"])]
subtitle = replacements["lib/CMakeFiles/StormByte-Multimedia.dir/private/StormByte/multimedia/ffmpeg/AVSubtitle.cxx.o"]
test_link = [args[0], "-stdlib=libc++", "-fsanitize=undefined", "-shared-libsan", out + "/persistent.o", out + "/helpers.o", subtitle, out + "/libStormByte-Multimedia.so.1.0.1"] + libs + ["-Wl,-rpath," + out, "-o", out + "/persistent-tests"]
lines += [out + "/persistent-tests: " + out + "/persistent.o " + out + "/libStormByte-Multimedia.so.1.0.1", "\tcd build/ubsan && " + shlex.join(test_link), ".PHONY: FORCE", "FORCE:"]
with open(out + "/local-barriers-build.log", "w") as log:
    result = subprocess.run(["make", "-s", "-f", "-", "-j" + str(len(os.sched_getaffinity(0)))], input="\n".join(lines) + "\n", text=True, env=env, stdout=log, stderr=subprocess.STDOUT)
print("BUILD", result.returncode)
if result.returncode:
    print(open(out + "/local-barriers-build.log").read()[-5500:])
raise SystemExit(result.returncode)
