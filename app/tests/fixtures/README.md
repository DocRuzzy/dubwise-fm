# Real saves from released firmwares (roadmap 9.13)

Each release adds one: its own simulator, built from its tag in a separate worktree, makes a project and saves it,
and its whole simulated flash (1 MiB, gzipped) is kept here. `tests/run_tests.sh` unpacks each into the build folder,
and a scenario (`app/tests/fixture_vX.Y.txt`) loads it into the current firmware with `flash_load out:...`: what users
actually have, not a scripted old save.

| File | Made by | What's in it |
|---|---|---|
| `v0.3-flash.bin.gz` | v0.3 (`b5cfa38`), `v0.3-make.script` | RIGHTEOUS DUB loaded, tempo 74.0, the bass's decay 30, the hats' level 60, scene 3 stored, SAVE, slot 2 saved |

To make the next one (from the repository's root):

    git worktree add /tmp/vX.Y vX.Y && ln -s "$PWD/base" /tmp/vX.Y/base   # (base/: the same pinned submodule)
    (cd /tmp/vX.Y && ./build.sh --host)
    /tmp/vX.Y/app/build/host/dubwise_host app/tests/fixtures/vX.Y-make.script /tmp/vX.Y/app/build/host
    gzip -9 app/tests/fixtures/vX.Y-flash.bin

(the script's last line: `flash_dump <this folder>/vX.Y-flash.bin`)
