# BASIC and Doom in the browser

The simulator core (`sim/src`) compiled to WebAssembly with Emscripten,
running the BASIC image (`basic/`) or Doom (`benchmarks/doom/`) on a web
page. This is the same
cycle-counting simulator as `fsim`. It runs in real time at 50 MHz, or as
fast as the browser allows.

```sh
make -C web          # needs emcc; builds BASIC first if needed
make -C web serve    # http://localhost:8000
make -C web deploy DEPLOY=user@host:/srv/www/basic/   # rsync to a web server
```

CI publishes every push to `main` to GitHub Pages:
https://n9wxu.github.io/fpga_cpu/

Serve it over HTTPS (or from localhost). Safari runs WebAssembly from
plain `http://` sites about 16 times slower, without compiling it.

The output is `web/build/`: `index.html`, `fsim.js`, `fsim.wasm`, and
`fsim.data` (both BASIC images and the examples), about 400 KB, plus
Doom's files, fetched only when Doom is picked: `doom.A.bin`, `doom.B.bin`
(about 0.5 MB each) and `DOOM.WAD.gz` (Freedoom: Phase 1, about 10 MB,
unpacked in the browser). Building Doom needs the LLVM toolchain, and the
first build downloads Freedoom (`benchmarks/doom/get_wad.sh`).
Any static web server can host it. Browsers will not load it from a
`file://` URL.

## The page

- **Program:** BASIC or Doom (`?prog=doom`). Doom runs from 14 MB of the
  simulator's expansion RAM, with the full palette. Arrow keys move, Ctrl
  fires, Space opens doors, Shift runs, Alt strafes, Esc is the menu. At
  50 MHz it draws about 6 frames per second; a browser too slow for real
  time runs it slower.
- **Screen:** click it, then type. The keys are those of
  [docs/io.md](../docs/io.md): Esc, the arrow keys, Home/End, F1 to F12, and
  Ctrl+letter. Pasting text types it.
- **Reset:** restarts BASIC.
- **Encoding:** A or B; changing it reboots with that image.
- **Speed:** 50 MHz real time or maximum. The status line shows the
  simulated MHz, MIPS, and the milliseconds each frame takes.
- **Colors:** how the hi-color (8-bit indexed) bitmap is shown: as the
  board's 3-bit VGA shows it, through a 2×2 dither, or with the full
  256-entry palette. `?color=full` selects the full palette.
- **Sound:** off until you press the button, as browsers require.
- **Disk:** the file-service folder `/disk`, stored in the browser's
  IndexedDB. It starts with `basic/examples`. Programs you `SAVE` stay
  there across reloads. Download and Upload copy files out and in.
  Upload names are cut down to 8.3 upper case.
- **LEDs:** the board LEDs.
- **Debug:** opens the fsim-gui windows below the screen. `?debug=1` opens
  them at load; the page remembers the choice.
  - Run (F5), Stop, Step (F11), Step over (F10) and Step out (Shift+F11).
    The keys work when the screen does not have the focus.
  - Registers, PC and flags, editable while stopped. Each pair is shown
    with the nearest symbol.
  - Disassembly with the program's symbols (BASIC, or Doom). Click a line to toggle a
    breakpoint. Type an address or symbol to go there; the wheel scrolls.
  - Memory as a hex dump, with the same go-to box.
  - Statistics: CPI, I-cache, branches, stall cycles and fusion candidates.
  - Board: the 7-segment display, LEDs, switches and push buttons.
  - Serial console: CON_OUT, plus a line input that feeds CON_IN.
- **`?keys=TEXT`:** types TEXT after boot (`\n` = Enter). For example
  `?keys=RUN "SPIRO.BAS"\n`.

`web.c` is the whole front end: boot, run a frame's worth of cycles and
render, keys (presses, releases for programs that turn them on with
`KBD_CTRL`, and the live modifiers), sound, and the debugger's state as
JSON. Run control (breakpoints and stepping) is `sim/src/dbg.c`, shared
with fsim-gui.
