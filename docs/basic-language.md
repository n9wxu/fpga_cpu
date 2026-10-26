# BASIC: a structured dialect and an integrated editor

Status: proposal. Nothing here is implemented yet. It describes how the
current interpreter in [`basic/`](../basic/README.md) can grow into a
clean, structured BASIC with a full-screen editor, without losing the
8-bit feel of booting to `READY.` and typing `10 PRINT "HI"`.

## Goals

- Remove the classic pain points: line numbers and renumbering, GOTO and
  GOSUB as the only way to structure a program, global-only variables,
  and editing one line at a time.
- Keep old line-numbered listings working unchanged.
- Keep the interpreter small and fast on a 16-bit CPU with no D-cache.
  Anything that costs a search at run time should be resolved once,
  before the program runs.

## What the current interpreter already gets right

Several hard parts are already done, and the proposal builds on them.

- Names are significant to 31 characters, so the old two-letter limit is
  already gone.
- The tokenizer stores each variable as a 16-bit index into a name
  table, so long names cost nothing at run time. BBC BASIC, by contrast,
  searches a linked list per initial letter, and its users run "crunch"
  tools to shorten names for speed.
- Numbers are stored in binary, so the run loop never converts text.
- Programs are saved as plain text listings. Every system that saved
  only a binary format (QuickBASIC's fast-load files, GFA BASIC's
  `.GFA`) eventually suffered from it, so text stays the canonical
  format.
- `str_t` already holds a 16-bit length. The 255-character limit lives
  only in a few checks in `eval.c` and `core.c`, so it can be raised
  without changing the heap.

## Lessons from other BASICs

| Dialect | What it fixed | Idea worth taking | What went badly |
|---|---|---|---|
| BBC BASIC | GOSUB spaghetti, global variables | `DEF PROC` with `LOCAL`; line numbers optional in later versions | PROC targets and `ENDCASE`/`ENDIF` found by scanning at run time, so speed degrades with program size |
| QuickBASIC / QBasic / QB64 | everything | `SUB`/`FUNCTION`, `TYPE`, `SELECT CASE`, line numbers as plain labels | one procedure per editor view hides the program's flow; `DECLARE` noise |
| MMBasic (Maximite, PicoMite) | line numbers and editing, on a microcontroller | full-screen editor; after an error, `EDIT` opens at the faulting line; a pointer table for labels and subs | 255-character strings |
| GFA BASIC (Atari ST) | line numbers and speed | syntax check and keyword normalising as each line is left; automatic indentation; folding procedures | silently turned variables into keywords; Find skipped folded code; binary files tied to one version |
| Commander X16 BASLOAD | line numbers, short names | labels and long names in a text source | it is a preprocessor, so errors report generated line numbers |
| Sinclair QL SuperBASIC | GOSUB, global variables | named loops with `EXIT name` | slow interpreter |
| AmigaBASIC, Yabasic | line numbers | a line number is just a label, so old programs run unchanged | |
| Locomotive BASIC | slow editing | `AUTO`, `RENUM`, `EVERY`/`AFTER` timer events | no procedures, single-line `IF` |
| PICO-8, TIC-80 | IDE weight on a tiny screen | procedure outline, next/previous procedure, go to line, help on the keyword under the cursor | |

The BASLOAD approach (a preprocessor in front of an unchanged old BASIC)
is the one to avoid. The program you run should be the program you
wrote, so that errors point at your own lines.

## Language

### Lines and labels

- A line number is optional. Unnumbered lines are stored in order, like
  lines in a text file.
- A line number, where present, is a numeric label. `GOTO 100` jumps to
  the line labelled 100, and the numbers do not have to be in order.
  This is the AmigaBASIC rule, and it is what lets old listings run
  unchanged.
- A name followed by `:` at the start of a line is a label:

  ```basic
  again:
    INPUT "Guess"; g%
    IF g% <> secret% THEN GOTO again
  ```

  This is unambiguous today, because a bare name is never a complete
  statement (assignment needs `=`).
- `GOTO`, `GOSUB`, `RESTORE` and `ON x GOTO` accept labels or numbers.

### Control structures

All of these are multi-line blocks. The single-line `IF ... THEN ...
ELSE` stays as it is.

```basic
IF x > 0 THEN
  PRINT "positive"
ELSEIF x < 0 THEN
  PRINT "negative"
ELSE
  PRINT "zero"
END IF

DO WHILE lives% > 0      ' also: DO UNTIL, DO ... LOOP WHILE/UNTIL, plain DO ... LOOP
  ...
LOOP

REPEAT
  k$ = INKEY$
UNTIL k$ <> ""

WHILE n% > 1 : n% = n% \ 2 : WEND

SELECT CASE key%
CASE 27
  EXIT DO
CASE ASC("A") TO ASC("Z"), ASC("_")
  ...
CASE ELSE
  BEEP
END SELECT
```

- `EXIT DO`, `EXIT FOR`, `EXIT WHILE`, `EXIT SUB` and `EXIT FUNCTION`
  leave the innermost block of that kind.
- A loop can be named (`DO outer` ... `LOOP outer`) so that `EXIT outer`
  leaves several levels at once. This is SuperBASIC's best idea, and it
  removes the last common reason for GOTO.

### Procedures

```basic
SUB box(x%, y%, w%, h%, c%)
  LOCAL i%
  PEN c%
  FOR i% = 0 TO h% - 1
    LINE x%, y% + i% TO x% + w% - 1, y% + i%
  NEXT
END SUB

FUNCTION clamp%(v%, lo%, hi%)
  IF v% < lo% THEN RETURN lo%
  IF v% > hi% THEN RETURN hi%
  RETURN v%
END FUNCTION

box 10, 10, 50, 20, 4
PRINT clamp%(score%, 0, 999)
```

- A `SUB` is called by name, like a built-in statement, with no `CALL`
  and no parentheses. That keeps the QL feeling that procedures extend
  the language. `CALL a` keeps its current meaning: run machine code.
- Parameters pass by value. `BYREF` before a parameter passes a variable
  instead, so a SUB can return results. Arrays always pass by reference.
- `LOCAL` variables are created fresh on each call. `STATIC` variables
  keep their value between calls. Recursion works, limited only by the
  BASIC stack.
- A `FUNCTION` returns with `RETURN expr`. The type suffix on its name
  gives the result type.
- Names not declared `LOCAL` or `STATIC` inside a procedure are globals,
  as in BBC BASIC. `OPTION EXPLICIT` makes every undeclared name an
  error; it is off by default.
- `DEF FN` stays for compatibility.

### Types

- Keep the sigils: `%` for 16-bit integer, `$` for string, none for
  float. They are part of the 8-bit feel, and they let the tokenizer
  know a variable's type without declarations.
- Add `&` for 32-bit integers. The CPU handles them well with `ADC`/`SBC`
  and the pointer adder, and loops over the 1 MB address space or large
  scores need them.
- Raise the string limit to 65,535 characters.
- `CONST name = expr` for named constants, folded at RUN time.
- Records (`TYPE ... END TYPE`) are worth adding later, once procedures
  exist. They are not needed for the first version.

### Events

`EVERY frames GOSUB label` and `AFTER frames GOSUB label`, from
Locomotive BASIC, suit games. They depend on the CPU's interrupt design,
which is still open, so they come last. On the current polled hardware
they can be checked once per statement against the frame counter.

## Editor

The current C64-style screen editor stays as the immediate-mode prompt.
`EDIT` (or a function key) switches to a full-screen editor over the
program in memory, and `Esc` returns to the prompt.

- **Error to line.** After any error, `EDIT` opens with the cursor on the
  faulting line and the message on the status line. MMBasic users rate
  this above every other feature, and it is the single most important
  part of the editor.
- **Check each line as it is left.** The editor tokenizes the line when
  the cursor leaves it. That checks syntax, converts keywords to upper
  case and expands `?` to `PRINT`. A bad line is marked, not locked:
  GFA BASIC's rule that you cannot leave a bad line gets in the way of
  large edits. A name that collides with a keyword is reported, never
  silently turned into the keyword.
- **Indentation is computed, not stored.** `LIST` and the editor indent
  from the block structure, so the stored program holds no layout
  spaces and every listing is formatted the same way.
- **Whole program in one view.** Unlike QuickBASIC, the editor shows one
  continuous text. A procedure list (`F2`) jumps to any `SUB` or
  `FUNCTION`, and `Shift-F2`/`Ctrl-F2` step to the next or previous one.
  Folding procedure bodies can come later; if it is added, Find must
  search inside folded code.
- **The usual editing keys:** find, find next and replace; go to line or
  label; mark, copy, cut and paste a block; one level of undo; `F5` to
  save and run.
- **Status line:** file name, current procedure, line and column, and
  free memory.
- **Help on the keyword under the cursor** if ROM space allows.
- **Colour.** Keywords, strings, comments and labels in different text
  colours. The text layer already has per-cell foreground colours.

The line editor still works at the prompt: typing `10 PRINT "HI"` stores
a numbered line, and `AUTO`, `RENUM`, `LIST` and `DELETE` keep working.

## Storage

### In memory

The line format changes very little:

```
[len lo][len hi][label lo][label hi] tokens 0
```

- The second field becomes a numeric label. 0 means the line has no
  number. (Line 0 is therefore not available, which no real program
  uses.)
- Name labels, `SUB` and `FUNCTION` headers are ordinary tokens whose
  name is an index into the existing name table.
- Keywords stay single-byte tokens. The new statements will push the
  keyword count past what fits in 0x80–0xFF, so one byte value becomes a
  prefix for a second page of 256 keywords.
- Block keywords (`IF` with a block, `ELSE`, `ELSEIF`, `CASE`, `DO`,
  `LOOP`, `WHILE`, `WEND`, `REPEAT`, `UNTIL`, `FOR`, `NEXT`, `EXIT`)
  carry a 2-byte operand slot. The tokenizer writes zero there, and the
  RUN-time pre-scan fills it in (see below).
- Inside a procedure, a reference to a `LOCAL`, `STATIC` or parameter
  carries its frame offset as well as its name index, so access is one
  add from the frame pointer and `LIST` can still print the name.

### On the SD card

Plain text `.BAS` stays the only required format. A tokenized save with
a version header can be added later for fast loading, but the text
format must always be able to round-trip a program.

## Interpreter changes

### Pre-scan at RUN

The biggest change is a single linear pass over the program when `RUN`
starts, rerun only if the program has been edited since. It builds:

- the label table: numeric and named labels to line addresses, sorted
  for binary search;
- the procedure table: name, parameter list and frame size;
- the DATA chain;
- each block keyword's operand slot: the offset of its partner (`ELSE`
  or `END IF`, the next `CASE`, `LOOP`, `WEND`, `UNTIL`, `NEXT`), so
  skipping a block is a jump, not a scan.

Mismatched blocks are reported here, before the program runs, with the
line and column for the editor to jump to.

This replaces today's `find_line`, which walks the program from the
start (or from the last line found) on every `GOTO` and `GOSUB`. BBC
BASIC's lazy approach, which searches the first time each procedure is
called and scans for `ENDIF` and `ENDCASE` every time, is the pattern
this avoids.

Immediate-mode statements (typed at the prompt without a number) run
without a pre-scan, so they resolve labels by search, which is fine at
typing speed.

### Call frames

The existing control stack (`ctl[]`, used for FOR and GOSUB) gains a
frame entry for each SUB and FUNCTION call: the return address, the
frame base, and the control-stack depth at entry. Locals live in a frame
area above it. `EXIT SUB`, `RETURN` from a function and errors unwind
the control stack to the depth recorded at entry, so a loop left early
never leaves stale FOR entries behind.

String locals need care: each frame slot is a string owner for the
collector, and the frame's strings are released when it is popped.

### Size

BBC BASIC fits a complete structured language with an assembler in
about 16–19 KB of Z80 code. In C on this CPU, the structured language
and the full-screen editor together should fit in roughly 48–96 KB.
That is an estimate, not a measurement; there is plenty of room either
way in 1 MB of SRAM.

## Compatibility

- `LOAD` accepts old numbered listings unchanged. Their numbers become
  labels.
- `RENUM` keeps its classic job for numbered programs.
- `UNNUM` removes every line number that no `GOTO`, `GOSUB`, `RESTORE`
  or `ON` refers to, and turns the referenced ones into labels such as
  `L100:`. This converts an old listing into the new style in one step.
- `GOSUB`/`RETURN`, `ON x GOTO`, `DEF FN` and the single-line `IF` all
  stay.

## Order of work

1. Optional line numbers and named labels, with the pre-scan and the
   label table. This is useful on its own and touches only the line
   format, `find_line` and the tokenizer.
2. Block `IF`, `DO`/`LOOP`, `WHILE`, `REPEAT`, `SELECT CASE` and `EXIT`,
   with partner offsets filled in by the pre-scan. Computed indentation
   in `LIST`.
3. `SUB` and `FUNCTION`, with `LOCAL`, `STATIC`, `BYREF` and call frames.
4. The full-screen editor, starting with error-to-line and the
   procedure list.
5. 32-bit `&` integers, long strings, `CONST`, `UNNUM`.
6. `TYPE` records, and `EVERY`/`AFTER` once interrupts are defined.

Each step ships with new `tests/*.bas` cases, and the existing tests
must keep passing, which keeps the old dialect honest throughout.

## Sources

- [BBC BASIC for Windows manual: general information (line numbers, labels, crunch)](https://www.bbcbasic.co.uk/bbcwin/manual/bbcwin2.html)
- [BBC BASIC (Z80) manual, Annex D: storage format](https://bbcbasic.co.uk/bbcbasic/mancpm/annexd.html)
- [RISC OS Open forum: Basic V performance](https://riscosopen.org:8443/forum/forums/11/topics/3904?page=1)
- [Making Agon BASIC go faster](https://lemmings.info/making-agon-basic-go-faster/)
- [QB64 help: View and Search keys](https://git.qb64.dev/QB64/qb64/src/commit/1a3939679c586d5d1a70bb54d716d76119b3d3bb/internal/help/View-and-Search-Keys.md)
- [QuickBASIC (Wikipedia)](https://en.wikipedia.org/wiki/QuickBASIC)
- [MMBasic overview](https://mmbasic.com/overview.html)
- [PicoMite User Manual](https://embdev.net/attachment/588774/PicoMite_User_Manual.pdf)
- [CircuitGizmos: MMBasic EDIT](https://circuitgizmos.com/gizmoblog/mmbasic-language/programming-and-debugging/edit)
- [BASLOAD ROM README](https://github.com/stefan-b-jakobsson/basload-rom)
- [MEGA65 wiki: Eleven walkthrough](https://mega65.atlassian.net/wiki/spaces/MEGA65/pages/34209812/Eleven+Walkthrough)
- [SuperBASIC (Wikipedia)](https://en.wikipedia.org/wiki/SuperBASIC)
- [Locomotive BASIC (Wikipedia)](https://en.wikipedia.org/wiki/Locomotive_BASIC)
- [Amiga Basic (Wikipedia)](https://en.wikipedia.org/wiki/Amiga_Basic)
- [ST NEWS: Your Second GFA BASIC Manual, the editor](https://st-news.com/issues/st-news-volume-9-issue-2/ed-lit/your-second-gfa-basic-manual-volume-1/2-the-editor)
- [START review of GFA BASIC 3 (1989)](https://www.atarimagazines.com/startv3n6/gfa_basic3.php)
- [OSS Blog: Yabasic](https://www.ossblog.org/yabasic-yet-another-basic/)
- [PICO-8 manual](https://www.lexaloffle.com/dl/docs/pico-8_manual.html)
- [TIC-80 wiki: code editor](https://github-wiki-see.page/m/nesbox/TIC-80/wiki/Code-Editor)
