#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Dubwise FM contributors
# Roadmap 9.2 (the prerelease assessment's finding 2): fm1-base's builds compile the o2 units (every file in
# app/engine/) with -w, so their warnings are never seen; a dangling else lived behind that (9.1). This compiles each
# with -Wall -Wextra for syntax only and fails on the kinds that hide bugs: a dangling else, a comment inside a
# comment, missing parentheses. The rest (an initialiser's {0}, a generated table's braces) are listed, not failed.
# control.c is part of the UI's unity build, not a unit of its own: skipped.
cd "$(dirname "$0")/.." || exit 1
CC=${CC:-cc}
fail=0
log=$(mktemp)
for f in app/engine/*.c; do
    [ "$(basename "$f")" = control.c ] && continue
    $CC -fsyntax-only -std=c99 -Wall -Wextra -Wdangling-else -Wcomment -Wparentheses -DPLAT_HOST \
        -Iapp -Iapp/engine -Ibase/include -Ibase/lib "$f" 2>>"$log"
done
if grep -E "\[-W(dangling-else|comment|parentheses)\]" "$log"; then
    echo "FAIL warnings that hide bugs in app/engine (above)"
    fail=1
fi
n=$(grep -c "warning:" "$log")
echo "  engine warnings: $n in all (only dangling-else, comment and parentheses fail)"
rm -f "$log"
exit $fail
