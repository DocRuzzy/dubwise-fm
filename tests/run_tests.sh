#!/bin/sh
# Host-simulator tests: no JieLi toolchain needed. Runs every app/tests/*.txt scenario except screens.txt, then
# the scenario's WAV checks: lines "#wav FILE CHECK ..." (tests/wavstat.py, on FILE in app/build/host).
set -e
cd "$(dirname "$0")/.."
python3 base/tools/build.py app --host
OUT=app/build/host
for t in app/tests/*.txt; do
    [ "$(basename "$t")" = screens.txt ] && continue
    echo "== $t"
    $OUT/dubwise_host "$t" $OUT
    grep '^#wav ' "$t" | while read -r _ f checks; do
        python3 -I tests/wavstat.py "$OUT/$f" $checks
    done
done
