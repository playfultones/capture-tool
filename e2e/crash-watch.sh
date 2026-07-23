#!/bin/sh
# Attach lldb to the running app and block until it stops (crash / fatal
# signal), then dump all-thread backtraces to out/crash-bt.txt and detach.
# Run in the background alongside a scenario or the driver:
#
#   ./crash-watch.sh [pid] [outfile]
#
# pid defaults to out/app.pid (written by the harness on launch). A clean app
# exit simply ends the lldb session with no backtrace.

set -eu
here="$(cd "$(dirname "$0")" && pwd)"
pid="${1:-$(cat "$here/out/app.pid")}"
out="${2:-$here/out/crash-bt.txt}"

exec lldb -p "$pid" --batch \
  -o continue \
  -k 'thread backtrace all' \
  -k 'detach' 2>&1 | tee "$out"
