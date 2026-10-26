# I/O map and memory layout

Status: draft, version 0.1. The simulator implements every register here.
The hardware will implement them in the FPGA, or forward them to the
RP2350 over the link UART (files, sound).

All registers are 16 bits wide and sit at even addresses in
`F00000–F0FFFF`. A 32-bit register is a lo/hi pair; a 32-bit store (`STP`)
writes the low half first. There are no interrupts yet, so software polls.

## System (F00000)

| Address | R/W | Name | Function |
|---|---|---|---|
| F00000 | W | `CON_OUT` | serial console: write a character |
| F00002 | W | `EXIT` | stop the simulation; the value is the exit code (hardware: ignored) |
| F00004 / F00006 | R | `CYCLE_LO/HI` | CPU cycle counter |
| F00008 | R | `CON_IN` | next serial input character; 0xFFFF if none is waiting, 0xFFFE when the input has ended (simulator: stdin closed). Never blocks. |
| F0000A | R | `FRAME` | frame counter, +1 at the start of each vertical blank (60 Hz) |
| F0000C | R | `VSTAT` | bit 0 = in vertical blank; bits 15:4 = current scan line (0–749) |

## Keyboard (F00100)

| Address | R/W | Name | Function |
|---|---|---|---|
| F00100 | R | `KBD_DATA` | next key from the queue, 0xFFFF if none (reading removes it) |
| F00102 | R | `KBD_MOD` | live modifier state: bit 0 shift, 1 ctrl, 2 alt |

Key codes are ASCII for printable keys. Others:

| Code | Key | Code | Key |
|---|---|---|---|
| 3 | Ctrl-C (break) | 0x80–0x83 | Up, Down, Left, Right |
| 8 | Backspace | 0x84 / 0x85 | Home / End |
| 9 | Tab | 0x86 / 0x87 | Page Up / Page Down |
| 13 | Enter | 0x88 | Insert |
| 27 | Esc | 0x90–0x9B | F1–F12 |
| 0x7F | Delete | 1–26 | Ctrl-A … Ctrl-Z |

On hardware, PS/2 scan codes are translated into this queue by FPGA
logic.

## Video (F00200)

The output is 1280×720 at 60 Hz. Every layer is 640×360 logical pixels,
shown doubled in both directions. The layers, from back to front, are:

1. Backdrop
2. Bitmap
3. Sprite buffer
4. Text

| Address | R/W | Name | Function |
|---|---|---|---|
| F00200 | RW | `VID_CTRL` | Bit 0: text on. Bit 1: bitmap on. Bit 2: bitmap hi-color (0 = 8-color). Bit 3: sprite layer on. Bit 4: shown sprite buffer (0/1). Bit 5: text cursor on. |
| F00202 | RW | `VID_BACKDROP` | backdrop color 0–7 |
| F00204 / F00206 | RW | `VID_TEXT` | text buffer address |
| F00208 / F0020A | RW | `VID_BMP` | bitmap address |
| F0020C / F0020E | RW | `VID_SPR` | sprite buffer 0 address; buffer 1 is at +115200 |
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
| 15 | stipple: draw the background as a 50% checkerboard of the background color and transparency |

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

**Sprite buffer.** It has the same format as the 8-color bitmap, and T = 1
is transparent. The CPU (later the blitter) draws sprites into the hidden
buffer, then flips bit 4 of `VID_CTRL` during vertical blank.

## Sound (F00400)

There are 3 voices, each 8 bytes apart (voice n is at `F00400 + 8n`).

| Offset | Name | Function |
|---|---|---|
| +0 | `SND_FREQ` | frequency in Hz; 0 = silent |
| +2 | `SND_VOL` | volume 0–15 |
| +4 | `SND_WAVE` | 0 square, 1 triangle, 2 sawtooth, 3 noise |

On hardware the RP2350 generates the sound (PWM), and these writes are
forwarded to it over the link.

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
| F00514 / F00516 | R | `FS_SIZE` | file size from `DIR_NEXT` |

| Cmd | Name | Uses | Result |
|---|---|---|---|
| 1 | `OPEN` | name, mode | handle |
| 2 | `CLOSE` | handle | 0 |
| 3 | `READ` | handle, buf, len | bytes read (0 = end of file) |
| 4 | `WRITE` | handle, buf, len | bytes written |
| 5 | `DELETE` | name | 0 |
| 6 | `DIR_OPEN` | — | 0 |
| 7 | `DIR_NEXT` | buf (at least 13 bytes) | 1 = name written to buf (NUL-terminated) and size in `FS_SIZE`; 0 = no more entries |

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

## Memory layout (C runtime convention)

| Range | Use |
|---|---|
| 000000–000FFF | reserved, kept unused so null pointers stand out |
| 001000– | program: code, then read-only data, data, and BSS |
| … up to 07FFFF | heap upward from the end of BSS, stack downward from 080000 |
| 080000–0B83FF | sprite buffers 0 and 1 (2 × 115,200) |
| 0C0000–0F83FF | bitmap (230,400 bytes; 8-color mode uses the first 115,200) |
| 0FC000–0FCF9F | text buffer (4,000 bytes) |
| 0FD000–0FFFFF | free |
