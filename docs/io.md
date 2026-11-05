# I/O map and memory layout

Status: draft, version 0.1. The simulator implements every register here.
The hardware will implement them in the FPGA, or forward them to the
RP2350 over the link UART (files, sound).

All registers are 16 bits wide and sit at even addresses in
`F00000–F0FFFF`. A 32-bit register is a lo/hi pair; a 32-bit store (`STP`)
writes the low half first. Devices raise interrupts through the controller at F00700 (below).

## System (F00000)

| Address | R/W | Name | Function |
|---|---|---|---|
| F00000 | W | `CON_OUT` | serial console: write a character |
| F00002 | W | `EXIT` | stop the simulation; the value is the exit code (hardware: ignored) |
| F00004 / F00006 | R | `CYCLE_LO/HI` | CPU cycle counter |
| F00008 | R | `CON_IN` | next serial input character; 0xFFFF if none is waiting, 0xFFFE when the input has ended (simulator: stdin closed). Never blocks. |
| F0000A | R | `FRAME` | frame counter, +1 at the start of each vertical blank (60 Hz) |
| F0000C | R | `VSTAT` | bit 0 = in vertical blank; bits 15:4 = current scan line (0–749) |
| F00010 / F00012 | R | `INSTRET_LO/HI` | instructions retired (SR3 has only the low 16 bits). The halves are read separately: read HI, LO, HI again and retry if HI changed |

## Keyboard (F00100)

| Address | R/W | Name | Function |
|---|---|---|---|
| F00100 | R | `KBD_DATA` | next key from the queue, 0xFFFF if none (reading removes it) |
| F00102 | R | `KBD_MOD` | live modifier state: bit 0 shift, 1 ctrl, 2 alt |
| F00104 | RW | `KBD_CTRL` | bit 0: also queue key releases, as the key's code with bit 15 set (0 after reset). Games use it to know which keys are held. |

Key codes are ASCII for printable keys. Others:

| Code | Key | Code | Key |
|---|---|---|---|
| 3 | Ctrl-C (break) | 0x80–0x83 | Up, Down, Left, Right |
| 8 | Backspace | 0x84 / 0x85 | Home / End |
| 9 | Tab | 0x86 / 0x87 | Page Up / Page Down |
| 13 | Enter | 0x88 | Insert |
| 27 | Esc | 0x90–0x9B | F1–F12 |
| 0x7F | Delete | 1–26 | Ctrl-A … Ctrl-Z |

On the Spartan-3 these registers read "no key". Software reads the PS/2
ports (F00300) instead, and `fpga/lib/ps2.c` decodes them into these
codes (`kbd_get()`, `kbd_mod()`). A release is the PS/2 break code. A release carries the code its
press had (Ctrl-A released is 1 | 0x8000), and printable keys are released
as their unshifted character.

## Video (F00200)

The output is 1280×720 at 60 Hz. Every layer is 640×360 logical pixels,
shown doubled in both directions. The layers, from back to front, are:

1. Backdrop
2. Bitmap
3. Sprite buffer
4. Text

| Address | R/W | Name | Function |
|---|---|---|---|
| F00200 | RW | `VID_CTRL` | Bit 0: text on. Bit 1: bitmap on. Bit 2: bitmap hi-color (0 = 8-color). Bit 3: sprite layer on. Bit 4: shown sprite buffer (0/1). Bit 5: text cursor on. Bit 6: sprite buffer is 8 bpp (0 = 4 bpp). Bit 7: bitmap 16-color (when bit 2 is 0). |
| F00202 | RW | `VID_BACKDROP` | backdrop color 0–7 |
| F00204 / F00206 | RW | `VID_TEXT` | text buffer address |
| F00208 / F0020A | RW | `VID_BMP` | bitmap address |
| F0020C / F0020E | RW | `VID_SPR` | sprite buffer 0 address; buffer 1 follows it (+115,200 at 4 bpp, +230,400 at 8 bpp) |
| F00210 | W | `PAL_INDEX` | palette index for the next `PAL_DATA` write |
| F00212 | W | `PAL_DATA` | palette color, RGB565 (see below); the write auto-increments `PAL_INDEX` |
| F00214 | RW | `VID_SCROLLX` | bitmap horizontal scroll, pixels (wraps at 640) |
| F00216 | RW | `VID_SCROLLY` | bitmap vertical scroll, lines (wraps at 360) |
| F00218 | RW | `VID_CURSOR` | text cursor cell, row × 80 + column |

The power-on value of every register is 0. The C runtime's start-up code
sets the layout below, with text and cursor on.

**Colors.** Colors 0–7 are the same in every mode. The bits are R, G, B
(bit 2 = red):

| Value | Color | Value | Color |
|---|---|---|---|
| 0 | black | 4 | red |
| 1 | blue | 5 | magenta |
| 2 | green | 6 | yellow |
| 3 | cyan | 7 | white |

**Text.** 80 × 25 cells with an 8×14 font, filling 640×350. The bottom
10 lines are always transparent. Each cell is 16 bits:

| Bits | Meaning |
|---|---|
| 7:0 | character code (CP437 glyphs) |
| 10:8 | foreground color |
| 13:11 | background color |
| 14 | 1 = background is transparent, so lower layers show through |
| 15 | stipple: draw the background as a 50% checkerboard of the background color and transparency (on a true-color display, such as HDMI or the simulator's `--true-color`: a 50% blend of the background color with what is beneath) |

The buffer is 4000 bytes, row-major.

**Bitmap, 8-color mode.** 4 bits per pixel, `[T][R][G][B]`. Two pixels per
byte, with the left pixel in the high nibble. A row is 320 bytes and the
buffer is 115,200 bytes. T = 1 makes the pixel transparent, so the
backdrop shows through.

**Bitmap, hi-color mode.** 8 bits per pixel, each indexing a 256-entry
palette. A row is 640 bytes and the buffer is 230,400 bytes.

- `PAL_DATA` is RGB565: bits 15:11 red, 10:5 green, 4:0 blue.
- The 18-bit hardware palette takes the top bits of each field.
- On the 3-bit base hardware the palette color is reduced with a 2×2
  ordered dither. The simulator draws it the same way by default; the
  `--true-color` option shows the real palette.
- Palette index 0 is transparent.

**Bitmap, 16-color mode** (bit 7 set, bit 2 clear). 4 bits per pixel as
in 8-color mode (left pixel in the high nibble, 320-byte rows, 115,200
bytes), direct color with no palette: 0–7 are the eight colors, 9–15 are
colors 1–7 at half brightness, and 8 is transparent. On the 3-bit VGA a
half-bright pixel is a 50% checkerboard of the color and black (like a
stippled text background); on a true-color display it is half intensity.

**Video tiers.** The Spartan-3 (fpga/rtl/ramsys.v, system.v) is the low
tier: no hi-color and no palette (bits 2 and 6 are ignored and `PAL_DATA`
does nothing), so the bitmap is 8-color or 16-color. The Tang Nano 20K is
the high tier, with every mode. `fsim --no-hicolor` shows the low tier.

**Sprite buffer.** There are two formats, chosen by bit 6 of `VID_CTRL`:

- 4 bpp (bit 6 = 0): the same format as the 8-color bitmap, and T = 1 is
  transparent. Each buffer is 115,200 bytes.
- 8 bpp (bit 6 = 1): the same format as the hi-color bitmap, through the
  same 256-entry palette, and index 0 is transparent. Each buffer is
  230,400 bytes. This mode works whatever the bitmap's mode, so sprites
  can use the full palette over an 8-color bitmap too.

Buffer 1 starts right after buffer 0. The CPU or the blitter draws
sprites into the hidden buffer, then flips bit 4 of `VID_CTRL` (it takes
effect at the next frame, see "Double buffering").

### Video extensions (F00220, high tier only)

The Tang Nano 20K adds a second playfield, a per-line scroll table,
hardware sprites and a blitter. On the low tier these registers do nothing
and read 0. The layers, back to front:

1. Backdrop
2. Playfield B when it is behind (`VID_CTRL2` bit 1 = 0), else playfield A
3. The other playfield
4. Sprite buffer (`VID_CTRL` bit 3)
5. Hardware sprites (`VID_CTRL2` bit 3), unless bit 4 puts them behind the sprite buffer
6. Text

| Address | R/W | Name | Function |
|---|---|---|---|
| F00220 | RW | `VID_CTRL2` | Bit 0: playfield B on. Bit 1: playfield B in front of playfield A. Bit 2: line scroll on. Bit 3: hardware sprites on. Bit 4: hardware sprites behind the sprite buffer. |
| F00222 | R | `VID_STAT` | Bit 0: a flip is pending (a buffer register was written and has not yet taken effect). Bit 1: vertical blank. |
| F00224 / F00226 | RW | `VID_BMPB` | playfield B address (a multiple of 4) |
| F00228 | RW | `VID_SCROLLXB` | playfield B horizontal scroll, pixels (wraps at 640) |
| F0022A | RW | `VID_SCROLLYB` | playfield B vertical scroll, lines (wraps at 360) |
| F0022C / F0022E | RW | `VID_LSCR` | line-scroll table address (a multiple of 4) |
| F00230 | RW | `SPR_INDEX` | sprite attribute word for `SPR_DATA`: sprite × 4 + word (0–255) |
| F00232 | RW | `SPR_DATA` | the attribute word at `SPR_INDEX`; a write increments `SPR_INDEX` |
| F00234 | R | `SPR_OVER` | lines in the last frame that had more than 16 sprites |

**Playfield A** is the bitmap of the "Video" section: `VID_BMP`,
`VID_SCROLLX`, `VID_SCROLLY`, in any bitmap mode. **Playfield B** is
always 8 bpp through the palette (the hi-color format: 640-byte rows,
230,400 bytes) with index 0 transparent, whatever playfield A's mode is.

**Double buffering.** `VID_BMP`, `VID_BMPB`, `VID_SPR`, `VID_LSCR` and
`VID_CTRL` bit 4 (the shown sprite buffer) are taken at the start of each
frame, during vertical blank: a write takes effect at the next frame,
never part way down the screen. `VID_STAT` bit 0 is 1 from the write until
the frame that uses it starts; when it is 0 the old buffer is no longer
shown and can be drawn into. The scroll registers and everything else act
at once (the scroll at the next line).

**Line scroll.** With `VID_CTRL2` bit 2 set, `VID_LSCR` points at 360
32-bit entries, one per logical line. Entry *y* holds two 16-bit offsets,
each 0–639: the low half is added to `VID_SCROLLX` and the high half to
`VID_SCROLLXB` for line *y* (modulo 640; to move a line left by *n*, use
640 − *n*). Parallax bands, waves and roads are a table
write; the table can be changed while the frame is drawn (each line reads
its entry just before it is shown).

**Hardware sprites.** 64 sprites, each 8, 16 or 32 pixels wide and 1–32
lines tall, 8 bpp through the palette with index 0 transparent. A lower
sprite number is in front of a higher one. Up to 16 sprites can be on one
line: the 16 lowest-numbered there are shown and the others are not on
that line (`SPR_OVER` counts such lines). Each sprite has four attribute
words, read and written through `SPR_INDEX` / `SPR_DATA`:

| Word | Bits | Meaning |
|---|---|---|
| 0 | 9:0 | X: the sprite's left column is X − 32 (so 0–31 is partly off the left edge) |
| 1 | 8:0 | Y: its top line is Y − 32 |
| 1 | 15:11 | height − 1 (1–32 lines) |
| 2 | 15:0 | pixel data address, bits 15:0 (a multiple of 4) |
| 3 | 3:0 | pixel data address, bits 19:16 |
| 3 | 12 | flip horizontally |
| 3 | 14:13 | width: 0 = 8, 1 = 16, 2 = 32 pixels |
| 3 | 15 | on |

The pixel data is rows of width bytes, top to bottom. Moving a sprite is
a write of word 0 or 1; the change shows from the next line drawn.

### Blitter (F00800, high tier only)

The blitter copies or fills a rectangle of bytes in memory while the CPU
runs: a row of `BLT_W` bytes, `BLT_H` times, stepping each address by its
stride after each row. It works on any bytes, so it draws into every bitmap
format (8 bpp: one byte a pixel; 4 bpp: two pixels a byte).

| Address | R/W | Name | Function |
|---|---|---|---|
| F00800 / F00802 | RW | `BLT_SRC` | source byte address |
| F00804 / F00806 | RW | `BLT_DST` | destination byte address |
| F00808 | RW | `BLT_SSTRIDE` | source row step in bytes (signed) |
| F0080A | RW | `BLT_DSTRIDE` | destination row step in bytes (signed) |
| F0080C | RW | `BLT_W` | bytes in a row, 1–4095 |
| F0080E | RW | `BLT_H` | rows, 1–65535 |
| F00810 | RW | `BLT_KEY` | bits 7:0: the colour key |
| F00812 | RW | `BLT_FILL` | bits 7:0: the fill byte |
| F00814 | W | `BLT_CMD` | starts a blit: bit 0: 0 copy, 1 fill; bit 1: keyed (a copy leaves the destination byte alone where the source byte equals `BLT_KEY`) |
| F00816 | R | `BLT_STAT` | bit 0: busy |

Rows are done in order, left to right: a copy whose destination overlaps
its source must have the destination before the source (or use a stride
that walks away from it). Writing `BLT_CMD` while busy does nothing; wait
for bit 0 of `BLT_STAT` to clear. The CPU's cache sees the blitter's
writes, so the CPU can read what was blitted once the blit is done.

## Sound (F00400)

There are 3 voices, each 8 bytes apart (voice n is at `F00400 + 8n`).

| Offset | Name | Function |
|---|---|---|
| +0 | `SND_FREQ` | frequency in Hz; 0 = silent |
| +2 | `SND_VOL` | volume 0–15 |
| +4 | `SND_WAVE` | 0 square, 1 triangle, 2 sawtooth, 3 noise |

On the Tang Nano 20K these are voices 0-2 of the audio engine
(docs/audio.md). On the Spartan-3 (the low tier, `fpga/rtl/audio_lite.v`)
they are a small synthesizer of their own, with a streaming player beside
it, on an I2S DAC (for example a PCM5102A) on the A2 expansion connector:
pin 4 (E6) bit clock, pin 5 (D5) word clock, pin 6 (C5) data, 3.3 V,
ground on pin 1. The output is 16-bit stereo at 48,828 Hz (50 MHz / 1024).

- Each voice: a square (±full scale), triangle (half scale), sawtooth, or
  noise (a 15-bit LFSR, x^15 + x^14 + 1, shifted when bit 19 of the
  voice's 24-bit phase rises), times `VOL` × 5 / 256, so three voices at
  full volume do not clip.
- The streaming player plays a ring buffer of stereo frames from RAM (a
  32-bit word a frame: the left sample in the low half, the right in the
  high, 16-bit signed). The program keeps it filled behind `STR_POS`.
- The mix is the three voices plus the stream × `STR_VOL` / 256, clipped
  to 16 bits.
- Only `STR_POS` reads back; the other registers read 0 there.
- `fsim --no-hicolor` models this device bit for bit, and `--wav FILE`
  writes its output.

| Address | R/W | Name | Function |
|---|---|---|---|
| F00420 | W | `STR_CTRL` | bit 0: play (0 → 1 starts at the buffer's first frame). Bits 3:1: n − 1, each buffer frame plays for n output frames (48,828 / n Hz: 48.8, 24.4, 16.3, 12.2 ... kHz) |
| F00422 | W | `STR_VOL` | stream volume, 0x100 = 1.0 (bits 8:0, up to 0x1FF) |
| F00424 / F00426 | W | `STR_ADDR_LO/HI` | the buffer's byte address (a multiple of 4), bits 19:0 |
| F00428 | W | `STR_LEN` | the buffer's length in frames, 1-65535; the player loops around it |
| F0042A | R | `STR_POS` | the frame being fetched, 0 .. `STR_LEN` − 1 (it is heard two output frames later) |

## Files (F00500)

This is a file service backed by the RP2350's SD card (FAT). The simulator
serves a host directory instead (`--disk DIR`, default `./disk`). Names
are flat 8.3-style names with no directories, compared without regard to
case.

To issue a command:

1. Set up the arguments.
2. Write the command number to `FS_CMD`.
3. Poll `FS_STATUS` until bit 15 (busy) is clear. The simulator finishes
   immediately.
4. Read `FS_RESULT`.

| Address | R/W | Name | Function |
|---|---|---|---|
| F00500 | W | `FS_CMD` | command (table below) |
| F00502 | R | `FS_STATUS` | bit 15 busy |
| F00504 | RW | `FS_HANDLE` | file handle 0–7 |
| F00506 | RW | `FS_MODE` | open mode: 0 read, 1 write (create or truncate), 2 append |
| F00508 / F0050A | RW | `FS_NAME` | address of the NUL-terminated name |
| F0050C / F0050E | RW | `FS_BUF` | buffer address |
| F00510 | RW | `FS_LEN` | byte count |
| F00512 | R | `FS_RESULT` | signed: a value of 0 or more is the result; a negative value is an error |
| F00514 / F00516 | R | `FS_SIZE` | file size from `DIR_NEXT` or `SIZE` |
| F00518 / F0051A | RW | `FS_POS` | byte position for `SEEK` |

| Cmd | Name | Uses | Result |
|---|---|---|---|
| 1 | `OPEN` | name, mode | handle |
| 2 | `CLOSE` | handle | 0 |
| 3 | `READ` | handle, buf, len | bytes read (0 = end of file) |
| 4 | `WRITE` | handle, buf, len | bytes written |
| 5 | `DELETE` | name | 0 |
| 6 | `DIR_OPEN` | — | 0 |
| 7 | `DIR_NEXT` | buf (at least 13 bytes) | 1 = name written to buf (NUL-terminated) and size in `FS_SIZE`; 0 = no more entries |
| 8 | `SEEK` | handle, pos | 0; the next `READ` or `WRITE` starts at byte pos |
| 9 | `SIZE` | handle | 0, with the open file's size in `FS_SIZE` |

`FS_LEN` is 16 bits and `FS_RESULT` is signed, so one `READ` or `WRITE`
moves at most 32,767 bytes. On the RP2350, `SEEK` and `SIZE` map onto
FatFs's `f_lseek` and `f_size`.

Errors: −1 not found, −2 bad handle, −3 no free handle, −4 I/O error, −5 bad
name.

## Board I/O (F00600)

The Spartan-3 Starter Board's own LEDs, switches, buttons and display.

| Address | R/W | Name | Function |
|---|---|---|---|
| F00600 | RW | `LEDS` | 8 LEDs, bit 0 = LD0 |
| F00602 | R | `SWITCHES` | 8 slide switches, bit 0 = SW0 |
| F00604 | R | `BUTTONS` | 4 push buttons, bit 0 = BTN0 |
| F00606 | RW | `SEG7` | four hex digits on the 7-segment display, digit 3 is leftmost |
| F00608 | RW | `SEG7_CTRL` | bits 3:0 = decimal points, bits 7:4 = digit blank |

GPIO, I2C and SPI on the expansion connectors come later, at F00700.


## Interrupts (F00700)

One interrupt line to the CPU: high while any source is pending and
enabled. The CPU takes it between instructions when its IE bit is set
(docs/t1.md, "Interrupts"): it keeps the address of the next instruction
in EPC, clears IE and jumps to 0x000010; the handler saves registers and
flags itself. `RETI` returns to EPC and sets IE. A program puts a jump to its handler at 0x000010: an
unconditional long branch is the two parcels `0xF700 | offset[22:16]`,
`offset[15:0]`, with offset = (target − 0x10) / 2.

| Address | R/W | Name | Function |
|---|---|---|---|
| F00700 | RW | `IRQ_PEND` | pending sources; write 1s to clear them |
| F00702 | RW | `IRQ_EN` | sources that raise the line |
| F00704 | W | `IRQ_SET` | set pending bits (a software interrupt; tests) |

| Bit | Source |
|---|---|
| 0 | vertical blank started (`FRAME` moved) |
| 1 | PS/2 port A: a byte arrived |
| 2 | PS/2 port B: a byte arrived |
| 7 | software only (`IRQ_SET`) |

The registers are 8 bits (bits 15:8 read 0). A source stays pending until
cleared, so a handler clears what it handled. Sources for serial, SD/WiFi
and sound will take bits 3-6 as they are added. `fpga/lib/irq.c` has an
entry stub (flags and registers saved) and a handler for each source.

## PS/2 ports (F00300)

Two PS/2 ports, A and B, for a keyboard and a mouse in either. The
hardware (`fpga/rtl/ps2.v`) moves whole bytes each way; resetting,
identifying and decoding the devices is software's (`fpga/lib/ps2.c`).
On the Spartan-3, A is the board's connector and B is on the A2
expansion connector (pin 7 clock, pin 8 data).

| Address | R/W | Name | Function |
|---|---|---|---|
| F00300 | RW | `PS2A_DATA` | read: the received byte, 0xFFFF if none (reading takes it); write: send a byte to the device |
| F00302 | R | `PS2A_STAT` | bit 0: a byte waits; bit 1: sending; bit 2: the last send failed (no device clocked it) |
| F00304 | RW | `PS2B_DATA` | port B, as `PS2A_DATA` |
| F00306 | R | `PS2B_STAT` | port B, as `PS2A_STAT` |

- **Receiving:** a port holds one byte, and it holds the clock line low
  until the byte is read. The device waits to send its next byte, so
  bytes are never overrun. Frames with a bad parity or stop bit are
  dropped. Each byte raises the port's interrupt source (bit 1 for A,
  bit 2 for B).
- **Sending:** a write starts the host-to-device transfer. The port holds
  the clock low for about 155 µs, then clocks the byte out on the
  device's clock and takes the device's acknowledge. A send that the
  device never clocks ends after 17–33 ms with bit 2 set: nothing is
  plugged in. A write while a byte waits discards that byte. The device's
  reply (FA, then any data) arrives as received bytes.
- **Detecting a device:** reset it (`FF`): it answers FA, then AA after
  its self-test (a mouse adds its ID, 00). Then identify it (`F2`): a
  keyboard answers FA AB 83 and a mouse FA 00 (03 or 04 for a wheel
  mouse). A device plugged in later sends AA by itself.

`ps2_init()` in `fpga/lib/ps2.c` does this for both ports. It turns on
the mouse's reporting (`F4`), and it decodes set-2 scan codes into the
`KBD_DATA` key codes (`kbd_get()`) and mouse packets into movement
(`ps2_mouse_get()`), from the interrupts.

The simulator plugs a keyboard into A and a mouse into B (`--ps2 A,B`
with `kbd`, `mouse` or `none`). The GUI's keyboard and mouse drive them;
headless, `--ps2keys` types and `--ps2mouse "dx,dy,buttons;..."` moves.

## Memory layout (C runtime convention)

| Range | Use |
|---|---|
| 000000–000FFF | reserved, kept unused so null pointers stand out |
| 001000– | program: code, then read-only data, data, and BSS |
| … up to 0533FF | heap upward from the end of BSS, stack downward from 053400 |
| 053400–0C3BFF | sprite buffers 0 and 1 (2 × 230,400, room for 8 bpp) |
| 0C3C00–0FBFFF | bitmap (230,400 bytes; 8-color mode uses the first 115,200) |
| 0FC000–0FCF9F | text buffer (4,000 bytes) |
| 0FD000–0FFFFF | free |
| 100000–7FFFFF | Tang Nano 20K with T1F: the rest of the 8 MB SDRAM, free (docs/t1f.md, `AW24`); fsim `--xram 7` |
