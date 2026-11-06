#!/usr/bin/env bash
set -euo pipefail

if [[ $# != 5 && $# != 6 ]]; then
	echo "usage: bash test/measure_preview.sh RUNNER FILE WALL_SECONDS SEEK_SECONDS OUTPUT_DIR [auto|software]" >&2
	exit 2
fi
runner=$1
source=$2
seconds=$3
seek=$4
output=$5
policy=${6:-auto}
if [[ $policy != auto && $policy != software ]]; then
	echo "decode policy must be auto or software" >&2
	exit 2
fi
prefix=
if [[ $# == 6 ]]; then
	prefix="$policy-"
fi
mkdir -p "$output"
for mode in baseline contention; do
	for repeat in 1 2 3; do
		load_pid=
		if [[ $mode == contention ]]; then
			(sleep 1; "$runner" --cpu-load 2 "$(nproc)") &
			load_pid=$!
		fi
		status=0
		result="$output/$prefix$mode-$repeat.csv"
		log="$output/$prefix$mode-$repeat.log"
		"$runner" --measure "$source" "$seconds" "$seek" "$policy" \
			>"$result" 2>"$log" || status=$?
		if [[ -n $load_pid ]]; then
			wait "$load_pid"
		fi
		if [[ $status != 0 ]]; then
			cat "$log" >&2
			exit "$status"
		fi
	done
done
