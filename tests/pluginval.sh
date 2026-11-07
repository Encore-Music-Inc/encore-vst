#!/usr/bin/env bash
# Runs pluginval (Tracktion's plugin validator) on each plugin given, the way a
# DAW would load it: state save and restore, parameters, odd buffer sizes and
# sample rates, editor open and close, threading. Prints the end of the log of
# any plugin that fails, and exits non-zero if one did.
#
#   tests/pluginval.sh <pluginval binary> <strictness 1-10> <plugin>...
#
# Strictness 5 is the level hosts expect plugins to pass; 10 is the most
# thorough. Full logs go to pluginval-logs/.
set -uo pipefail
bin="${1:?usage: pluginval.sh <pluginval> <strictness> <plugin>...}"
level="${2:?strictness}"
shift 2
out="pluginval-logs"
mkdir -p "$out"
failed=0
for p in "$@"; do
  name="$(basename "$p")"
  echo "== pluginval (strictness $level): $name"
  start=$SECONDS
  if "$bin" --strictness-level "$level" --validate-in-process --timeout-ms 900000 \
       --validate "$p" > "$out/$name.txt" 2>&1; then
    echo "   passed in $((SECONDS - start)) s"
  else
    echo "::error::pluginval failed for $name"
    tail -60 "$out/$name.txt"
    failed=1
  fi
done
exit "$failed"
