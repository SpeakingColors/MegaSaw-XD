# MegaSaw XD

<img src="assets/icon.svg" width="96" align="right" alt="icon">

A bigger SuperSaw oscillator for the **KORG minilogue xd**, free to use.

Two layers of JP-8000 style supersaw, up to **11 voices per layer**, each layer with its own waveform (saw, square, pulses, glass, or a breathy "Ah" vocal), plus analog-style drift. Made for trance, hardstyle and UK hardcore leads.

It is the merge of [HyperSaw](https://github.com/SpeakingColors/hypersaw-xd) (waves, octave, balance, Vox Ah) and [HyperSawX](https://github.com/SpeakingColors/super-duper-stuff) (more voices, drift).

**Download:** the latest `MegaSaw_v1.0.zip` from [Releases](https://github.com/SpeakingColors/MegaSaw-XD/releases), or the unit file directly: [`dist/MegaSaw.mnlgxdunit`](dist/MegaSaw.mnlgxdunit)

**Listen:** [`audio/megasaw_demo.wav`](audio/megasaw_demo.wav) plays the same riff on three presets.

> Status: version 1.0, tested on a PC against HyperSaw (see "How it was tested"). Not yet tested on real minilogue xd hardware. If it glitches on your synth, turn **Voices** down and tell us.

## Install (Windows or Mac)
1. Download the zip from Releases and unzip it. Do not unzip or rename the `.mnlgxdunit` file inside.
2. Open **KORG minilogue xd Sound Librarian** and connect the xd by USB.
3. Drag `MegaSaw.mnlgxdunit` into a free **User Oscillator** slot.
4. Send it to the synth.
5. On the xd: **Multi Engine, USR, MegaSaw**.

## Install on Linux (not tested by us yet)
The Sound Librarian has no Linux version, so use KORG's command line tool `logue-cli`:
1. Get it from the [KORG logue SDK](https://github.com/korginc/logue-sdk) folder `tools/logue-cli`: run `get_logue_cli_linux.sh` (64-bit Linux only). On an immutable distro such as Bazzite, run it inside a distrobox.
2. Check the file: `logue-cli check -u MegaSaw.mnlgxdunit`
3. List the MIDI ports: `logue-cli probe -l`. Use the input port ending in **KBD/KNOB** and the output port ending in **SOUND** (the other ports are the old MIDI DIN jacks and will not work).
4. Load it into a spare slot: `logue-cli load -u MegaSaw.mnlgxdunit -s 15 -i <in> -o <out>`
5. If `load` fails, that is a known issue on Linux (KORG's issue #37). Tell us what you see.

## Controls
| Control | What it does |
|---|---|
| SHAPE | Detune (JP-8000 curve). LFO and mod routing can sweep it |
| SHIFT + SHAPE | Mix: center voice vs the side voices |
| Wave A | Waveform of layer A (table below). If it is Off, layer A plays Supersaw |
| Wave B | Waveform of layer B (screen 1 = layer B off) |
| Oct B | Octave of layer B vs layer A, -2 to +2 (the screen shows `%`, read it as octaves) |
| Layer Bal | -100 = only A, 0 = equal, +100 = only B |
| Voices | Voices per layer: -2 = 3, -1 = 5, **0 = 7**, +1 = 9, +2 = 11 (shows `%`, read it as the step) |
| Drift | Each voice slowly wanders in pitch, like analog (0 to 100%) |

The xd screen shows Wave A and Wave B as **1 to 10** (it counts one higher than the wave's internal number):

| Screen | Wave | Screen | Wave |
|---|---|---|---|
| 1 | Off | 6 | Triangle |
| 2 | Supersaw | 7 | Sine |
| 3 | Square | 8 | Glass 1 (bright) |
| 4 | Pulse 25% | 9 | Glass 2 (bell) |
| 5 | Pulse 12% | 10 | Vox Ah (breathy vocal) |

If both layers are Off, layer A plays Supersaw so you never get silence. Loudness stays the same whichever number of voices you pick.

## Starter sounds
- **Classic JP lead:** Wave A Supersaw, Wave B Off, Voices 0, Shape ~60%, Shift+Shape ~70%
- **Mega stack:** Wave A Supersaw, Wave B Supersaw, Oct B -1, Voices +2, Bal 0, Drift ~30%, Shape ~70%
- **Hardstyle Ah lead:** Wave A Vox Ah, Wave B Supersaw, Bal -20, Voices +1, Drift ~30%, Shape ~70%, Shift+Shape ~60%, short decay, sustain ~35%
- **Pure choir:** Wave A Vox Ah, Wave B Off, Shape ~25%, slow attack
- **Trance pad:** Wave A Supersaw, Wave B Glass 1, Oct B +1, Bal -30, Drift ~40%

Pairs well with [SawWiden](https://github.com/SpeakingColors/sawwiden-xd), our stereo widener mod FX (the xd oscillator output is mono, so width comes from the FX).

## Glitches or dropouts?
The xd plays up to 4 notes, and each note runs its own copy of the oscillator. At full size (2 layers, 11 voices) MegaSaw costs about the same as HyperSaw's heaviest setting, so a busy 4-note chord can be too much. Lower **Voices** first, then turn layer B off. Vox Ah and Glass waves cost more than saws.

## How it was tested
On a PC with the test in [`test/`](test): every wave, every voice count and the extreme settings render without NaN, clipping or DC offset, and loudness varies by less than 1 dB between 3 and 11 voices. Against HyperSaw: the same level within 0.2 dB (Saw and Vox Ah), and MegaSaw at 2x11 saws runs about as fast as HyperSaw at 3x7 saws (PC timing, so only a relative guide).
The unit was built with the KORG logue SDK and arm-none-eabi-gcc 13.2 (the SDK's own scripts fetch an older compiler, 5.4 or 10.3).

## Build it yourself
1. Clone the [KORG logue SDK](https://github.com/korginc/logue-sdk) with `--recursive` and install its ARM toolchain.
2. Copy this repo's files into `logue-sdk/platform/minilogue-xd/megasaw`.
3. Run `make install` in that folder. `megasaw.mnlgxdunit` appears next to the Makefile.

Run the PC test: `cd test && g++ -O2 -std=c++11 -I. -o t test_megasaw.cpp && ./t`

Detune and mix curves follow Adam Szabo, "How to Emulate the Super Saw" (2010).

## License
MIT for our code (`megasaw.cpp`, `test/`); KORG SDK template files in `tpl/` and `ld/` keep their BSD 3-Clause license. See [LICENSE](LICENSE).

Not affiliated with or endorsed by KORG Inc. minilogue is a trademark of KORG Inc.
