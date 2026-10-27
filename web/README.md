# BASIC in the browser

The simulator core (`sim/src`) compiled to WebAssembly with Emscripten,
running the BASIC image (`basic/`) on a web page. This is the same
cycle-counting simulator as `fsim`. It runs in real time at 50 MHz, or as
fast as the browser allows.

```sh
make -C web          # needs emcc; builds BASIC first if needed
make -C web serve    # http://localhost:8000
make -C web deploy   # copy to the retro container (DEPLOY=user@host:dir to change)
```

The deployed copy is at https://retro.julicher.net/basic/. Serve it over
HTTPS (or from localhost): Safari runs WebAssembly from plain `http://`
sites about 16 times slower, without compiling it.

The output is `web/build/`: `index.html`, `fsim.js`, `fsim.wasm`, and
`fsim.data` (both BASIC images and the examples), about 280 KB in all.
Any static web server can host it. Browsers will not load it from a
`file://` URL.

## The page

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
- **`?keys=TEXT`:** types TEXT after boot (`\n` = Enter). For example
  `?keys=RUN "SPIRO.BAS"\n`.

`web.c` is the whole front end: boot, run a frame's worth of cycles and
render, keys, and sound. The serial console (CON_IN/CON_OUT) is not
connected in the browser.
