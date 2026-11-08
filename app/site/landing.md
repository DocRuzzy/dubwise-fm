# DUBWISE

Dubwise FM will be a reggae/dub groovebox for the M-VAVE FM-1, where the mixing desk is the instrument: quantised
mutes and solos, send throws into a feedback tape echo and a kickable spring, and a stepped Big Knob filter. This
early build is still fm1-base's template, without Bluetooth. Every control shows its name on the screen as you use
it, the keys play a simple voice with velocity and pressure, your settings save, MIDI goes in and out over USB and
comes in over TRS, and it works with a screen reader.

![DUBWISE while a key is held: C4 in the header with V 90 P 64, FX lit among the buttons, K3 at 3, the key filled](manual/img/screen-playing.png "DUBWISE on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). Play it right here, with sound. It runs the same code as the FM-1, so you don't
  need one.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains the screen, the controls, saving and MIDI.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/DocRuzzy/dubwise-fm) is under GPL-3.0-only. It's built on fm1-base (MPL-2.0) and
  felucca-bsp (MPL-2.0), which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. Dubwise FM is not affiliated with M-VAVE.
