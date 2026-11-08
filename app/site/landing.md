# DUBWISE

Dubwise FM will be a reggae/dub groovebox for the M-VAVE FM-1, where the mixing desk is the instrument: quantised
mutes and solos, send throws into a feedback tape echo and a kickable spring, and a stepped Big Knob filter. This
early build has the DUB page: the white keys mute and solo eight channels, and each change waits for the next beat or
bar line so it lands on time. There are no voices yet (test tones stand in), and it works with a screen reader.

![The DUB page playing: bar 2.3, the hats muted, the bass soloed, the organ's mute waiting for the next bar](manual/img/screen-playing.png "DUBWISE on the FM-1's screen")

This is version {{VERSION}}. You install it at your own risk, and the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/). Play it right here, with sound. It runs the same code as the FM-1, so you don't
  need one.
- [Install](install/) it from Chrome or Edge, with the FM-1 connected by USB.
- The [Manual](manual/) explains the screen, the controls, saving and MIDI.
- [Download {{PKG}}](firmware/{{PKG}}) to use the command-line installer: `python3 tools/fm1_install.py {{PKG}}`.
- The [Source](https://github.com/DocRuzzy/dubwise-fm) is under GPL-3.0-only. It's built on fm1-base (MPL-2.0) and
  felucca-bsp (MPL-2.0), which come from Felucca by Leo Kuroshita, Hügelton Instruments.

M-VAVE and FM-1 are trademarks of their owners. Dubwise FM is not affiliated with M-VAVE.
