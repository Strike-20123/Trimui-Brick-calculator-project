#!/bin/sh
DIR="$(dirname "$0")"
cd "$DIR" || exit 1
export LD_LIBRARY_PATH="$DIR:/usr/trimui/lib:$LD_LIBRARY_PATH"
exec ./calculator
