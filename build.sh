#!/bin/sh
# Device package (needs the JieLi Linux toolchain and AC79 SDK files; see base/README.md).
#   ./build.sh                 -> app/build/dubwise.fwsc
#   ./build.sh --release 0.1   -> app/build/dubwise-0.1.fwsc
#   ./build.sh --host          -> host simulator (no toolchain needed)
#   ./build.sh --web           -> browser emulator (emscripten)
set -e
cd "$(dirname "$0")"
exec python3 base/tools/build.py app "$@"
