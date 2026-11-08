#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# SessionStart: check out base/ (fm1-base, with felucca-bsp in base/bsp/) so tests/run_tests.sh works at once.
set -euo pipefail
cd "${CLAUDE_PROJECT_DIR:-.}"

if [ ! -f base/tools/build.py ]; then
  git submodule update --init --recursive
fi

# The host build's screenshots need Pillow (CI installs it too); cloud sessions only, and only if missing
if [ "${CLAUDE_CODE_REMOTE:-}" = "true" ] && ! python3 -I -c 'import PIL' 2>/dev/null; then
  python3 -m pip install --break-system-packages Pillow
fi
