import concurrent.futures
import json
import os
from pathlib import Path
import re
import subprocess

root = Path("/home/stormbyte/Software/StormByte/StormByte/multimedia")
output = Path(__file__).parent / "static-audit"
output.mkdir(exist_ok=True)
database = root / "build/pgo-instr"
entries = json.loads((database / "compile_commands.json").read_text())
files = sorted({entry["file"] for entry in entries})
checks = "-*,clang-analyzer-*,bugprone-*,performance-*,portability-*"


def analyze(item):
    index, file = item
    result = subprocess.run(
        [
            "clang-tidy", "-p", str(database),
            f"-checks={checks}", "--system-headers",
            f"-header-filter=^{re.escape(str(root))}/(lib|test)/",
            "-quiet", file,
        ],
        capture_output=True, text=True,
    )
    log = output / f"{index:03d}.log"
    text = result.stdout + result.stderr
    log.write_text(text)
    diagnostics = [
        line for line in text.splitlines()
        if re.search(r":\d+:\d+: (warning|error):", line)
    ]
    return {
        "file": file, "exit": result.returncode,
        "log": str(log), "diagnostics": diagnostics,
    }


results = []
with concurrent.futures.ThreadPoolExecutor(max_workers=os.process_cpu_count()) as pool:
    for result in pool.map(analyze, enumerate(files)):
        results.append(result)
        print(f"{len(results)}/{len(files)} exit={result['exit']} "
              f"diagnostics={len(result['diagnostics'])} {result['file']}", flush=True)

(output / "results.json").write_text(json.dumps(results, indent=2))
diagnostics = sorted({line for result in results for line in result["diagnostics"]})
(output / "diagnostics.txt").write_text("\n".join(diagnostics) + "\n")
categories = {}
for line in diagnostics:
    match = re.search(r"\[([^]]+)\]$", line)
    category = match[1] if match else "unclassified"
    categories[category] = categories.get(category, 0) + 1
print(json.dumps({
    "files": len(files), "failed": sum(result["exit"] != 0 for result in results),
    "unique_diagnostics": len(diagnostics), "categories": categories,
}, indent=2))
