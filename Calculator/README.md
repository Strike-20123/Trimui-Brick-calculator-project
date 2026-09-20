# Calculator for TrimUI Brick (stock OS)

A D-pad and face-button driven calculator, built for the Brick's 1024x768
screen and its lack of a touchscreen. Standard and scientific modes share one
expression engine, so `=` evaluates whatever's on the display in either mode.

## Controls

| Input        | Action                                             |
|--------------|-----------------------------------------------------|
| D-pad        | Move the highlighted key (holding it repeats)        |
| A            | Press the highlighted key                            |
| B            | Backspace                                            |
| X            | Clear                                                |
| Y            | Insert `(` or `)` — picks whichever balances the expression |
| L1 / R1      | Switch Standard ↔ Scientific                         |
| Start        | Quit                                                 |

## Project layout

```
Calculator/
  config.json     stock OS app manifest (label + launch script)
  launch.sh       sets LD_LIBRARY_PATH and runs the binary
  Makefile        cross-compile (device) and host (PC test) build targets
  src/main.c      the whole app: input, expression parser, SDL2 rendering
  res/font.ttf    Share Tech Mono (SIL Open Font License, see res/OFL.txt)
```

After building, this **whole `Calculator` folder** — including the compiled
`calculator` binary — is what you copy to the device.

## 1. Build

You need an aarch64 cross toolchain that matches the Brick's libc, plus SDL2
+ SDL2_ttf headers/libs for that target. The simplest way to get one is
Shaun Inman's MinUI toolchain, which targets this exact hardware (it calls
the platform `tg5040`; the binary it produces runs fine on stock OS too,
since MinUI runs as an app on top of the same base OS/libc):

```bash
git clone https://github.com/shauninman/union-tg5040-toolchain
cd union-tg5040-toolchain
make shell
```

That drops you into a shell targeting the device. From there:

```bash
cd /path/to/workspace/Calculator   # wherever you mounted this project
make
```

This produces `calculator`, an aarch64 ELF linked against SDL2/SDL2_ttf.

**Testing on your PC first (optional but recommended):** install SDL2 and
SDL2_ttf dev packages for your OS (e.g. `apt install libsdl2-dev
libsdl2-ttf-dev` on Debian/Ubuntu, `brew install sdl2 sdl2_ttf` on macOS),
then from the `Calculator` folder run:

```bash
make host
./calculator-host
```

Use arrow keys to move the cursor, `Z`/Enter to press a key, `X`/Backspace,
`C` to clear, `V` for parentheses, `A`/`S`/Tab to switch modes, Escape to
quit.

## 2. Install on the Brick

1. Put the device in USB storage mode (or pull the microSD card).
2. Copy the entire `Calculator` folder to `SD_ROOT/Apps/` (so you end up
   with `Apps/Calculator/config.json`, `Apps/Calculator/calculator`, etc.).
3. Eject, reboot if needed, and open it from the **Apps** menu.

## Notes and things you might want to extend

- Backspace removes one character at a time — after tapping a scientific
  function like `sin(`, one press of B won't remove the whole token, only
  the trailing `(`.
- Trig functions (`sin`, `cos`, `tan`) work in degrees.
- `%` is postfix and divides the preceding value by 100 (so `50%` → `0.5`,
  `200+10%` → `200.1` — standard "percent of nothing in particular"
  calculator behavior, not "10% of 200"; adjust `doInsert`/the parser if you
  want the latter).
- The font is loaded from `res/font.ttf` relative to the working directory
  `launch.sh` sets, so keep `res/` alongside the binary.
- Want an app icon? Add an `icon.png` (or `icontop.png`) into the
  `Calculator` folder and reference it from `config.json` — see the format
  documented in https://github.com/trimui/assets_brick.
