#!/bin/sh
# TrimUI launcher. Logs everything to log.txt next to the binary so that if
# the app fails to start on the device you have something to read.

DIR="$(dirname "$0")"
cd "$DIR" || exit 1

# Prefer bundled libs if you ever drop any into ./lib, then the device's own.
export LD_LIBRARY_PATH="$DIR/lib:/usr/trimui/lib:/usr/lib:$LD_LIBRARY_PATH"
export HOME="$DIR"

./calculator > log.txt 2>&1
