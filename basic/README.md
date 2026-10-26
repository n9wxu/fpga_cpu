# basic

FPGA BASIC: a line-numbered BASIC with an 8-bit feel, written in portable C
for the fpga_cpu target (16-bit `int`, 32-bit pointers, soft float). It runs
on the development machine too, which is how it is tested.

```sh
make -C basic              # host build: basic/build/basic-host
make -C basic test         # feed tests/*.bas to the prompt, diff against tests/*.out
make -C basic check        # -Wconversion build, to catch 16/32-bit slips
basic/build/basic-host                       # interactive prompt
basic/build/basic-host basic/examples/SPIRO.BAS   # load, run, exit
```

The host build reads its environment:

| Variable | Effect |
|---|---|
| `BASIC_DISK` | the file directory (default `./disk`) |
| `BASIC_PPM` | write the bitmap and sprite layers to this PPM on exit |
| `BASIC_FASTFRAMES` | the frame counter advances on every read, so `VSYNC`/`PAUSE` don't wait |
| `BASIC_ARENA` | a smaller workspace, in bytes |

## Source layout

| File | Contents |
|---|---|
| `src/hal.h` | Hardware abstraction: console, keys, memory/registers, frames, files |
| `src/hal_host.c` | Host HAL. It emulates SRAM and the registers, keeps files in a directory, and writes the PPM dump. |
| `src/hal_fcpu.c` | Target HAL on the [docs/io.md](../docs/io.md) registers. The keyboard drives the screen editor; serial input is line-buffered. |
| `src/basic.h` | Shared types, tokens, I/O addresses |
| `src/core.c` | Name table, tokenizer, `LIST` detokenizer, program store, arrays, string heap and collector |
| `src/eval.c` | Expression evaluator and functions |
| `src/stmt.c` | Statements, run loop, prompt, files |
| `src/num.c` | Number formatting and parsing (float only, no libc) |
| `src/con.c` | 80×25 text console and screen editor |
| `src/gfx.c` | Bitmap drawing, software sprites, sound registers |

**Target build.** The target build compiles the shared files plus
`hal_fcpu.c`. It needs only `memcpy memmove memset memcmp strlen`,
`setjmp`/`longjmp`, and `sinf cosf tanf atanf expf logf sqrtf powf floorf
fabsf fmodf`. It uses no stdio, malloc or ctype. The arena runs from `_end`
to 16 KB below the stack top at `0x080000`.

## Using it

Type a line with a number to store it, or without one to run it now. A
number on its own deletes that line. `READY.` means the prompt is waiting.

- On the target, the keyboard edits the screen like a C64. Move the cursor
  anywhere and press Enter to re-enter that line, so you can `LIST`, change
  a line and press Enter.
- Ctrl-C stops a running program with `BREAK IN n`; `CONT` resumes it.

Keywords must be separated from names by a space or punctuation (`FOR I=1
TO 10`), except that a keyword may run straight into digits (`GOTO100`).
Lowercase is accepted and listed as uppercase; strings and `REM`/`DATA`
text keep their case.

## Values

| Type | Example | Notes |
|---|---|---|
| Float | `A`, `SCORE` | 32-bit IEEE, printed to 7 digits |
| Integer | `A%` | 16-bit, −32768..32767. `A% = X` stores `INT(X)`. |
| String | `A$` | up to 255 characters |

- Every character of a name is significant (up to 31). `A`, `A%`, `A$` and
  the array `A()` are four separate variables.
- Arrays: `DIM A(10, 5)` holds indices 0..10 × 0..5. An array used without
  `DIM` gets 0..10 in each dimension.
- Numbers: `12`, `.5`, `1.5E-3`, and hex `&HF00600`.
- Whole numbers below 16,777,216 print exactly. Others use 7 significant
  digits, with E notation outside 0.01 .. 9,999,999.
- Positive numbers print with a leading space, and every number with a
  trailing space (MS style).
- True is −1 and false is 0. `AND OR NOT` are bitwise on 16-bit values.

Operators, from highest precedence to lowest:

| Precedence | Operators |
|---|---|
| 1 (highest) | `^` |
| 2 | unary `-` |
| 3 | `* / \ MOD` |
| 4 | `+ -` |
| 5 | `= <> < > <= >=` |
| 6 | `NOT` |
| 7 | `AND` |
| 8 (lowest) | `OR` |

- `/` always gives a float. `\` is integer division.
- Integer operands stay integers through `+ - * \ MOD` while the result
  fits in 16 bits.
- `+` joins strings, and the comparisons work on strings.

## Statements

### Program

| Statement | Effect |
|---|---|
| `NEW` | clear the program and variables |
| `LIST [n] [n-m] [-m] [n-]` | list the program or a range of lines |
| `RUN [line]` / `RUN "file"` | clear the variables and run |
| `CONT` | resume after `STOP` or Ctrl-C |
| `END`, `STOP` | stop the program (`STOP` prints `BREAK IN n`) |
| `CLEAR` | clear the variables |
| `REM text` / `' text` | comment |

### Variables and data

| Statement | Effect |
|---|---|
| `[LET] v = expr` | assign |
| `DIM a(n[,m...])[, ...]` | create arrays |
| `DATA 1, "two", three` | values for `READ`; an unquoted item is trimmed |
| `READ v[, v...]` | read the next `DATA` values |
| `RESTORE [line]` | make `READ` start again, optionally at a line |
| `DEF FNname(x) = expr` | define a function of one numeric argument; call it as `FNname(3)` |

### Control

| Statement | Effect |
|---|---|
| `GOTO n` | jump to a line |
| `GOSUB n` … `RETURN` | call a subroutine and come back |
| `ON x GOTO a,b,c` / `ON x GOSUB a,b,c` | jump to the x-th line in the list |
| `IF c THEN stmts [ELSE stmts]` | conditional. `THEN n` and `ELSE n` jump to a line, and `IF c GOTO n` also works. A nested `IF` takes the nearest `ELSE`. |
| `FOR v = a TO b [STEP s]` … `NEXT [v[, w...]]` | loop. A loop whose start is already past its end runs zero times. Integer loop variables use integer math. |

### Input and output

| Statement | Effect |
|---|---|
| `PRINT` / `?` | print. `;` joins items, `,` moves to the next 16-column zone, `TAB(n)` moves to column n, `SPC(n)` prints spaces. |
| `INPUT ["prompt";] v[, v...]` | read values. A `;` after the prompt adds `? `; a `,` doesn't. Too few values ask again with `??`; a bad number prints `?REDO FROM START`. |
| `GET v$` / `GET v` | wait for one key. A numeric variable gets the key code. |
| `CLS` | clear the screen |
| `LOCATE row, col` | move the cursor; row 0–24, column 0–79 |
| `COLOR fg[, bg]` | set the text colors. bg 0–7 is solid; 8 (the default) is transparent. |

### Graphics

Colors 0–7 are black, blue, green, cyan, red, magenta, yellow, white.

| Statement | Effect |
|---|---|
| `TEXT` | text only: bitmap and sprites off |
| `GR` | 640×360 8-color bitmap on, cleared to transparent |
| `HGR` | hi-color bitmap on, with a default palette, cleared to 0. The palette is: 0 transparent, 1–7 primaries, 8 black, 9–15 dim primaries, 16–231 a 6×6×6 cube, 232–255 grays. |
| `GCLS [c]` | clear the bitmap (default transparent) |
| `BACKDROP c` | backdrop color behind every layer (default blue) |
| `PEN c` | drawing color: 0–15 in GR (8–15 are transparent), 0–255 in HGR |
| `PLOT x, y` | one pixel |
| `LINE x1,y1 TO x2,y2 [TO x3,y3 ...]` / `LINE TO x,y` | lines; `LINE TO` starts from the last point |
| `BOX x1,y1,x2,y2[,fill]` | rectangle, filled if fill is nonzero |
| `CIRCLE x,y,r[,fill]` | circle, filled if fill is nonzero |
| `PALETTE i, r, g, b` | set a hi-color palette entry, components 0–255 |
| `SCROLL x, y` | hardware scroll of the bitmap |

### Sprites

There are 16 sprites, numbered 0–15, each up to 32×32 pixels. Sprites
are drawn in software into the hidden sprite buffer, and changes appear on
the next `VSYNC`.

| Statement | Effect |
|---|---|
| `SPRITE n DEFINE w, h, s$[, s$...]` | define a sprite. Each character is one pixel: `0`–`7` a color, anything else transparent. Rows run left to right, top to bottom. |
| `SPRITE n DEFINE w, h READ` | read h rows from `DATA` |
| `SPRITE n AT x, y` / `SPRITE n OFF` | show a sprite at a position, or hide it |
| `VSYNC` | draw the sprites into the hidden buffer, wait for vertical blank, and swap. Without sprite changes it just waits one frame. |

### Sound

| Statement | Effect |
|---|---|
| `SOUND v, freq, vol[, frames]` | play voice 0–2 at a frequency in Hz and volume 0–15, optionally stopping after a number of frames |
| `WAVE v, w` | set a voice's waveform: 0 square, 1 triangle, 2 saw, 3 noise |
| `BEEP` | a short beep |
| `PAUSE frames` | wait (60 frames = 1 second) |

### Files

Names are 8.3. A name without a dot gets `.BAS`. Programs are saved as
plain text listings, so they can be edited on the host.

| Statement | Effect |
|---|---|
| `SAVE "name"` | save the program |
| `LOAD "name"` | load a program |
| `DIR` | list the files and their sizes |
| `DEL "name"` | delete a file |

### Memory and hardware

| Statement | Effect |
|---|---|
| `POKE a, b` / `DOKE a, w` | write a byte / a 16-bit word (even address) |
| `CALL a` | run machine code |

Anything in the 24-bit address space works, including the I/O registers in
[docs/io.md](../docs/io.md):

```basic
DOKE &HF00600, 255            : REM all 8 LEDs on
PRINT PEEK(&HF00602)          : REM the slide switches
DOKE &HF00606, &H1234         : REM 7-segment display shows 1234
IF PEEK(&HF00604) AND 1 THEN PRINT "BTN0"
```

## Functions

| Group | Functions |
|---|---|
| Math | `ABS INT SGN SQR SIN COS TAN ATN EXP LOG`. Angles are in radians. `INT` rounds down. |
| `RND(x)` | x > 0: the next random number in [0, 1). x = 0: the last one again. x < 0: reseed from x. |
| Strings | `LEN ASC VAL STR$ CHR$ HEX$ LEFT$(s,n) RIGHT$(s,n) MID$(s,start[,n]) INSTR(s,t[,start])` |
| Input | `INKEY$` is the next key, or `""` if none. It doesn't wait. |
| Screen | `POS(0)` is the cursor column. `POINT(x,y)` is a pixel's color, or −1 off-screen. |
| System | `TIME` is frames since power-on (60 per second). `FRE(0)` is free bytes, after a collection. `PEEK(a)` reads a byte, `DEEK(a)` a word. |

## Errors

Errors print classic messages such as `?SYNTAX ERROR IN 30`, `?TYPE
MISMATCH ERROR` and `?OUT OF DATA ERROR`. A program can't trap errors.

## Examples

All in `examples/`. Run them with `RUN "SPIRO"`.

| Program | What it shows |
|---|---|
| `SPIRO.BAS` | spirographs with `LINE TO` |
| `SPRITES.BAS` | a small game with sprites, arrow keys and sound effects |
| `MUSIC.BAS` | a three-voice tune from `DATA` |
| `HICOLOR.BAS` | hi-color palette bars and a palette-cycled disc |
| `PRIMES.BAS` | the sieve benchmark, timed with `TIME` |

## Internals

- **Lines** are stored tokenized: `[length][line number] tokens 0`.
  Keywords are bytes 0x80 and up; numbers are stored in binary; variable
  references are 16-bit indexes into a name table. That makes variable
  access O(1) with no searching at run time.
- **Memory:** the arena from the HAL holds the program, then the arrays
  growing up, then the string heap growing down from the top. Simple
  variables live in static tables.
- **Strings:** every heap string has exactly one owner (a variable, an
  array element or an expression-stack slot). The collector marks owners
  from a back-pointer trailer and slides live strings to the top in one
  pass.
- **Expressions** evaluate on an explicit value stack, so the collector can
  find every temporary.
