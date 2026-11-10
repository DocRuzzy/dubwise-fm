#!/bin/sh
# Host-simulator tests: no JieLi toolchain needed. Runs every app/tests/*.txt scenario except screens.txt, then
# the scenario's WAV checks: lines "#wav FILE CHECK ..." (tests/wavstat.py, on FILE in app/build/host).
set -e
cd "$(dirname "$0")/.."
python3 base/tools/build.py app --host
OUT=app/build/host
for g in app/tests/fixtures/*.bin.gz; do        # 9.13: the real saves of past releases, unpacked for flash_load
    gunzip -c "$g" > "$OUT/$(basename "$g" .gz)"
done
for t in app/tests/*.txt; do
    [ "$(basename "$t")" = screens.txt ] && continue
    echo "== $t"
    $OUT/dubwise_host "$t" $OUT
    grep '^#wav ' "$t" | while read -r _ f checks; do
        python3 -I tests/wavstat.py "$OUT/$f" $checks
    done
done
echo "== tests/warnings.sh (the engine with warnings on: 9.2)"
sh tests/warnings.sh || exit 1
echo "== tests/slot_transfer_test.py"
python3 tests/slot_transfer_test.py >/dev/null || { echo "FAIL slot transfer (run tests/slot_transfer_test.py)"; exit 1; }
echo "== tools/site/editor/fields.js (the web editor's table, from the firmware)"
python3 -I tools/site/editor/mkfields.py --check || exit 1
echo "== tests/patch_transfer_test.py"
python3 tests/patch_transfer_test.py >/dev/null || { echo "FAIL patch transfer (run tests/patch_transfer_test.py)"; exit 1; }
echo "== tests/shots_transfer_test.py"
python3 tests/shots_transfer_test.py >/dev/null || { echo "FAIL one-shots transfer (run tests/shots_transfer_test.py)"; exit 1; }
echo "  ok slot transfer"
echo "== tests/spring_alias_test.c"
cc -O2 -w -DPLAT_HOST -Iapp/engine -Ibase/include -Iapp tests/spring_alias_test.c -o $OUT/spring_alias_test -lm
$OUT/spring_alias_test || { echo "FAIL spring aliasing"; exit 1; }
echo "== base/tests/flash_ok_test.c (both FM-1 flash chips: Puya 856014 and UC25HQ80 B36014; docs/hardware.md)"
cc -O1 -I base/device base/tests/flash_ok_test.c -o $OUT/flash_ok_test
$OUT/flash_ok_test | tail -1
$OUT/flash_ok_test >/dev/null || { echo "FAIL the pinned fm1-base refuses one of the FM-1's flash chips"; exit 1; }
grep -q '"flash": \["856014", "B36014"\]' firmware.json || { echo "FAIL firmware.json does not list both flash chips"; exit 1; }
echo "  ok firmware.json lists both flash chips"
