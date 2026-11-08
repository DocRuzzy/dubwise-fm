#!/bin/sh
# Host-simulator tests: no JieLi toolchain needed. Runs every app/tests/*.txt scenario except screens.txt.
set -e
cd "$(dirname "$0")/.."
python3 base/tools/build.py app --host
for t in app/tests/*.txt; do
    [ "$(basename "$t")" = screens.txt ] && continue
    echo "== $t"
    app/build/host/dubwise_host "$t" app/build/host
done
