# XBOX Space Cadet Pinball

<div align=center>

<img src="https://github.com/Darkone83/XBSCPinball/blob/main/img/Screenshot.jpg" width=400> <img src="https://github.com/Darkone83/XBSCPinball/blob/main/img/Darkone83.png" width=400>

</div>

It is a native Original Xbox port of **Space Cadet Pinball**, based on the open-source SpaceCadetPinball project and adapted for the Xbox using native Direct3D, DirectSound, and XInput support.

## Features

- Native Original Xbox support
- 480i, 480p, 720p, and PAL 576i video output
- Full Space Cadet gameplay
- Original sound effects and MIDI music
- Controller rumble support
- Local settings and high-score saving
- Controller reconnect support
- Xbox-friendly controls and startup screen

## Requirements

You will need:

- A homebrew-capable Original Xbox
- The XBSCPinball release files
- The original **Full Tilt! Pinball - Space Cadet** game data

Game data can be obtained from:

https://archive.org/details/full_tilt_pinball

XBSCPinball does **not** include the original Full Tilt! Pinball game data.

The release package does include the SoundFont used for MIDI playback.

## Installation

1. Extract XBSCPinball to a folder on your Xbox hard drive.
2. Download the Full Tilt! Pinball data from the link above.
3. Copy the Space Cadet game data into the XBSCPinball folder.
4. Preserve the original `SOUND` folder structure.
5. Launch `default.xbe`.

A typical installation will contain:

```text
XBSCPinball/
├── default.xbe
├── CADET.DAT
└── SOUND/
    ├── TABA1.MID
    ├── TABA2.MID
    ├── TABA3.MID
    ├── gm.sf2
    └── original Space Cadet sound files
```

`gm.sf2` is included with the XBSCPinball release.

## Controls

| Control | Action |
|---|---|
| LT | Left Flipper |
| RT | Right Flipper |
| A | Plunger / Launch Ball |
| D-Pad Left | Nudge Left |
| D-Pad Right | Nudge Right |
| D-Pad Up | Nudge Table |
| Start | Pause / Resume |
| Y | New Game |
| Back + Start | Exit to Dashboard |

Rumble feedback is used for flippers, nudges, bumpers, targets, kickbacks, drains, tilt, and other table events.

## Save Data

XBSCPinball stores its settings and high scores locally on the Xbox hard drive.

- Settings: `D:\pinball.ini`
- High Scores: `D:\highscore.dat`

## Credits & Attribution

### XBSCPinball
Original Xbox port by **Darkone83**.

### SpaceCadetPinball
XBSCPinball is based on **SpaceCadetPinball** by **Andrey Muzychenko**.

SpaceCadetPinball is licensed under the **MIT License**.

### Original Game
**Full Tilt! Pinball** was developed by **Cinematronics, LLC** and published by **Maxis Software Inc.**

Space Cadet and the original game assets remain the property of their respective copyright holders.

### MIDI Playback
**fmidi**  
MIDI file parsing and playback library.  
Licensed under the **Boost Software License 1.0**.

### SoundFont Synthesis
**TinySoundFont** by **Bernhard Schelling**  
Based on SFZero by Steve Folta.  
Licensed under the **MIT License**.

### Included SoundFont
**Florestan Basic GM GS** by **Nando Florestan**  
Distributed as **Public Domain**.

### Xbox Development
Built using **RXDK** and the Original Xbox development environment maintained by the Xbox homebrew community and Team Resurgent.

## Disclaimer

XBSCPinball is an unofficial homebrew port and is not affiliated with Microsoft, Maxis, Electronic Arts, or the original game developers.

Original Full Tilt! Pinball game data is not distributed with XBSCPinball. Users are responsible for obtaining and using the required game data appropriately.
