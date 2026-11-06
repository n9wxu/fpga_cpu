# The ROM: BIOS, Toolbox and Shell (Tang Nano 20K)

The Tang Nano 20K boots from its onboard SPI flash, both halves: the FPGA configures itself
from the bitstream, and the FPGA's boot copier then loads the **ROM** from the same flash into
the SDRAM and starts the CPU in it. The ROM holds the **BIOS** (every service that touches a
register), the **Toolbox** (memory and utility managers) and the **Shell**; programs call them
through a slot table in low RAM. This is the first implementation of issue #18's design for the
Tang; the Spartan-3's parallel flash (A1) follows the same ABI later.

Sources: `rom/` (the ROM, its tools and tests), `tang/rtl/spiflash.v` (the boot copier and the
CPU's flash port), `rom/include/fcrom.h` (the ABI for programs).

## Boot

1. Power-up: the GW2AR-18 loads the bitstream from flash 000000.
2. The SDRAM controller initialises; ~1 ms later the boot copier (`spiflash.v`) reads the ROM
   image's header at flash **400000**. With the magic `FCRM` it copies the body to its load
   address (F80000) through the serial loader's write port, about 2 MB/s (16.9 MHz SPI; the
   17 KB ROM takes ~10 ms), and starts T1F at the entry. Without the magic the CPU waits for
   the serial loader, as before.
3. The ROM (fcc's crt0 sets its stack at FFFFF0 and clears its BSS) checks its copy against the
   CRC-32 in the flash header, writes the anchors and the slot table, starts the video (text
   layer), console, interrupts and sound, and runs the self-test, each line printed before its
   test and its result after:

   ```
   fpga_cpu ROM 1.0  BIOS and Toolbox  (Tang Nano 20K, T1F)
     ROM copy CRC ... ok
     RAM ... ok  7.5 MB for programs
     flash ... ok  0B4017, 2 file(s) on the ROM disk
     audio engine ... ok
   Starting BASIC: press a key for the shell
   ```

4. A rising arpeggio on success, a falling one on failure. With nothing failed and `BASIC` on
   the ROM disk, BASIC starts after two seconds unless a key arrives; otherwise the Shell.

- **S1 held at power-up:** no flash boot (the serial loader only), for recovery.
- **S1 and S2 held together for a second:** a reboot: T1 is held, the ROM is copied from the
  flash again and started (after a program overwrote the ROM's RAM, say).
- **The serial loader** (`tang/tools/run.py`: the sync word DE AD BE EF, `L`, `G`) works at
  any time, also while the ROM runs; a program it loads still finds the ROM's slots, and its
  exit goes back to the Shell.

## Memory

| Range | Use |
|---|---|
| 000010–000013 | interrupt vector: a long branch to the ROM's entry (owned by the BIOS) |
| 000020–00002F | anchors: `FCRM`, the warm restart, ROM version, board |
| 000100–0008FF | slot table, 512 × 4 bytes (function pointers), rewritten at every (re)start |
| 001000–77FFFF | programs (7.5 MB) |
| 780000–7FFFFF | the ROM's SDRAM, seen at **F80000–FFFFFF** (the SDRAM repeats every 8 MB) |

In the ROM's 512 KB: the image from F80000, its BSS, the system heap up to FF0000, the stack
below FFFFF0. A program that writes 780000–7FFFFF (`tang/progs/xramtest.c`, `vidhigh.c`)
overwrites the ROM: S1+S2 restores it.

## Calling the ROM

```c
#include <fcrom.h>                      /* fcc -I rom/include ... */
int main(void)
{
    if (!fcrom_present()) return 1;
    con_printf("ROM %x, %lu bytes free\n", tb_version(), mem_avail());
    snd_beep(880, 100);
    return 0;                           /* back to the Shell */
}
```

- Slot *n* is the function pointer at `000100 + 4n`; each call in `fcrom.h` is an inline
  function that loads it and calls it, an ordinary `CC_FCPU` call: no symbols, no link step
  against the ROM, and the same program works with any ROM that has its slots.
- `rom/include/slots.def` is the list (number, return type, name, parameters), used for both
  the programs' header and the ROM's table, so they cannot disagree. Append only.
- Fixed-width types only, so `--int32` programs call the same ROM. No variadic slot:
  `con_printf` is an inline shim over `con_vprintf`.
- An unfilled slot points at a panic that says so. `tb_patch(n, fn)` replaces a slot (one
  `STP`, so an interrupt sees the old or the new pointer); the table is rewritten at every
  return to the Shell, which undoes a program's patches.
- **Exit:** crt0's `exit` writes EXIT (the simulator stops; `run.py` ends), then, when the
  anchor says a ROM is there, jumps to the ROM's warm restart, which takes back its own stack,
  rewrites the slots and the vector and starts the Shell. Without a ROM it stays in a loop.

| Block | Slots | Calls |
|---|---|---|
| 0 System | 0–31 | `tb_version` `sys_board` `sys_warm` `sys_cycles` `sys_frame` `sys_wait` `sys_ram_top` `sys_panic` `irq_set` `irq_enable` `sys_ms` `sys_delay` `sys_buttons` `sys_leds` `tb_patch` |
| 1 Console | 32–63 | `con_putc` `con_puts` `con_write` `con_getc` `con_poll` `con_gets` `con_puthex` `con_putdec` `con_vprintf` `con_clear` `con_goto` `con_color` `con_target` |
| 2 Video | 64–95 | `vid_ctrl(_get)` `vid_ctrl2(_get)` `vid_text` `vid_bmp` `vid_spr` `vid_palette` `vid_scroll` `vid_wait_vbl` `vid_backdrop` `vid_reset` `vid_text_addr` |
| 3 Sound | 96–127 | `snd_tone` `snd_off` `snd_beep` `snd_ctrl(_get)` `snd_master` `snd_quiet` |
| 4 Flash | 128–159 | `fl_id` `fl_read` `fl_erase` `fl_program` `rd_count` `rd_entry` `rd_find` `rd_load` |
| 5 Memory | 160–191 | `mem_alloc` `mem_free` `mem_avail` |
| 6 Utility | 192–223 | `ut_memcpy` `ut_memset` `ut_memcmp` `ut_strlen` `ut_strcmp` `ut_stricmp` `ut_strcpy` `ut_crc32` `ut_rand` `ut_srand` |
| 7 Blitter | 224–255 | `blt_copy` `blt_fill` `blt_keycopy` `blt_wait` |
| 8 Shell | 256–287 | `sh_exec` `sh_run` |

Notes on the services:

- **Console:** the serial port and the text layer (80 × 25, scrolling, colours). Serial
  output is paced: the Tang's transmit queue holds 256 bytes and drops the rest, and the CPU
  cannot see its level, so the BIOS counts bytes against the cycle counter (87 µs a byte at
  115200 baud). Programs that print a lot with libc's `printf` lose characters; `con_*` do not.
- **Video and sound:** the registers are write-only (docs/io.md), so the BIOS keeps the copies
  (`vid_ctrl_get`, `snd_ctrl_get`, …) a program would otherwise read back.
- **Interrupts:** the BIOS's entry saves the flags and C's registers and calls each pending
  source's handler (`irq_set`); a handler clears its source's bit in `IRQ_PEND`.
- **Flash:** `fl_erase` and `fl_program` refuse anything below 400000 (the bitstream).
- **Memory:** a first-fit heap with merging, in the ROM's RAM (about 430 KB).

## The flash image (`rom/tools/mkrom.py`)

| Flash | Contents |
|---|---|
| 000000–3FFFFF | the bitstream (`cd tang && make flash`) |
| 400000 | header, 32 bytes, little endian: `FCRM`, body length, load address, entry, CRC-32 of the body, version, the ROM disk's offset, 0 |
| 400020 | the body: `rom/build/rom.bin`, linked at F80000 |
| 480000 | the ROM disk: `FCRD`, file count, 0, 0; 32-byte entries (name, 16 bytes; offset from 480000; length; load; entry); the files, each at a multiple of 16 |

The boot copier uses the header's first 16 bytes; the BIOS reads the CRC and the ROM disk
through the flash port. The ROM disk holds programs the Shell loads to their load address and
runs (`BASIC`, `ROMTEST`).

## Building and flashing

```
cd rom && make                 # build/rom.img: the ROM, BASIC and romtest on the ROM disk
make test                      # in fsim: boot and Shell, romtest through the slots, BASIC
cd ../tang && make bit         # the bitstream (with the flash port: --mspi_as_gpio)
make flash                     # the bitstream into the flash (000000)
make flash-rom                 # rom/build/rom.img at 400000
python3 tools/console.py -i    # the console (Ctrl-] ends)
```

`fsim --rom rom/build/rom.img --xram 7` runs the same image: F80000–FFFFFF is RAM there, the
image is loaded as the copier would, and F00900 is a model of the flash with the image in it
(`--rom-save` writes flash changes back). `tang/sim/system_tb.v +flash=rom.hex` boots the RTL
from a flash model.

## The Shell

`HELP`, `INFO`, `DIR`, `RUN name` (or just the name), `DUMP addr [len]`, `POKE addr byte…`,
`GO addr`, `BEEP [hz [ms]]`, `TEST`, `CLS`. Numbers are decimal, `$hex` or `0xhex`.

## Not yet

From issue #18, still to come: SD card boot (the Tang's TF slot has no interface in the RTL
yet), the settings sector, a flash updater over serial (`openFPGALoader` writes the flash for
now), the Events, Display, Graphics, Sprites, Files and Resources managers, BASIC inside the ROM
(it is a ROM disk program for now), and the Spartan-3's parallel flash.
