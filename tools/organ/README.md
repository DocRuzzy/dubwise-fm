# The B-3's Leslie data

`setbfree-whirl.txt` is setBfree's Leslie model (`b_whirl/whirl.c`) dumped as tables at 44.1 kHz: for 1024 rotor
angles, the delay of the sound to each microphone (horn and drum, forward and backward), the horn's 5-tap
angle-dependent filter, then the paths' spacings and phases and the three filters' coefficients.

- setBfree: https://github.com/pantherb/setBfree, Copyright (C) Fredrik Kilander, Will Panther, Robin Gareus, Ken
  Restivo and others, GPL-2.0-or-later.
- The dump is from setBfree-fm1 ("B3 ORGAN" for the FM-1, https://github.com/R-Jeb/setBfree-fm1, `data/` and
  `tools/dump_whirl.c`, GPL-2.0-or-later), copied unchanged.

`mkleslie.py` turns it into `app/engine/leslie_data.h` (512 angles, every other row: the firmware interpolates the
delays). Dubwise FM as a whole is GPL-3.0-only, which GPL-2.0-or-later material may join.

    python3 -I tools/organ/mkleslie.py
