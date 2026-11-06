# paint: an 8-colour paint program for the Spartan-3

A MacPaint/MS Paint-style program for the ramsys design (`fpga/rtl/ramsys.v`: T1S, 1 MB SRAM,
the 640x360 low-tier video, two PS/2 ports), driven by a PS/2 mouse (three buttons and a wheel)
and, if one is plugged in, a PS/2 keyboard. Either device can be in either port.

```sh
make -C apps/paint                     # build/paint.bin (needs llvm/fcc)
make -C fpga TOP=ramsys prog           # the ramsys design in the FPGA (or play build/ramsys.svf)
apps/paint/tools/paint.py run          # load paint and run it, with a white picture
apps/paint/tools/paint.py open x.png [--dither]   # run it with a picture (scaled to fit, 8 colours)
apps/paint/tools/paint.py save x.png   # the picture on the screen, as a PNG (paint then restarts with it)
make -C apps/paint fsim                # in fsim's GUI, as the Spartan-3 (the mouse and keyboard are the PS/2 ones)
make -C apps/paint test                # scripted sessions in fsim: out/*.png
```

## The screen

A desktop with windows, in the toolbox's look: white panels framed in black, a striped title bar
with a close box, a drop shadow.

- **Picture** windows show the picture at 1x, 2x, 4x or 8x (a grid at 8x), with scroll bars and
  a grow box. Window > New view opens up to two more views of the same picture, each with its
  own zoom and place, so one can be at 8x for detail while another shows the whole: drawing in
  one shows in all. The front picture window has the striped title bar.
- **Tools** (the 16 tools and the chosen tool's options) and **Colours** (foreground over
  background, the 8 colours, 16 patterns in the two colours) are palettes: they float above the
  pictures. Window > Tools / Colours hides and shows them; Window > Tidy windows puts every
  window back where it started.
- Drag a title bar to move a window, a picture window's grow box (bottom right) to size it;
  the window follows the pointer live. Click a close box to close a window.
- The screen is double-buffered and changes at the vertical blank: nothing is ever seen half
  drawn.
- The menu bar has File, Edit, Image, View, Window and Help, then the pointer's position (or a
  message) and the tool.

## The mouse

| | |
|---|---|
| Left button | draw in the foreground colour; choose |
| Right button | draw in the background colour (colours: choose the background) |
| Middle button | drag: move the view; click: show the whole picture, full screen (and back) |
| Wheel | over a picture: zoom in and out at the pointer; over a scroll bar: scroll; over the colours, the patterns, the tools or the options: step through them |

## The tools

| Tool | Key | |
|---|---|---|
| Select | S | drag a box; drag inside it to move it (right button: move a copy). Options: solid or clear (the background colour is see-through) |
| Colour picker | I | the colour under it becomes the foreground (right: background); back to the last tool |
| Pencil | P | one pixel |
| Brush | B | round, square or slanted, 1-12 pixels, in the pattern |
| Spray can | A | 4 sizes, in the pattern |
| Eraser | X | to the background colour, 4-16 pixels; right button: only the foreground colour |
| Paint bucket | F | fills the area of one colour with the pattern |
| Text | T | click, then type (Enter, Backspace); small or large, opaque or clear |
| Line | L | 1-8 wide; Shift: 0, 45 or 90 degrees |
| Curve | C | drag the line, then drag twice to bend it (right button: done) |
| Rectangle, rounded rectangle, ellipse | R, O, E | outline, filled with the pattern, or framed (outline with a background-colour inside); Shift: square or circle |
| Polygon | G | click the corners; double-click, right-click or click the first corner to close it |
| Magnifier | Z | left: zoom in, right: zoom out |
| Hand | H | drag the view |

Keys (with a PS/2 keyboard): 0-7 the foreground colour (Shift: background), `[` `]` the size,
`+` `-` zoom, arrows move the view, Tab the full-screen picture, Esc cancels, Delete clears
the selection, F1 help; Ctrl-Z/Y undo and redo, Ctrl-X/C/V cut, copy and paste, Ctrl-A select
all, Ctrl-N new, Ctrl-F/J flip, Ctrl-E invert, Ctrl-R foreground to background, Ctrl-G the grid.

Undo keeps each operation's changed rectangle in a log of 1.9 bitmaps' worth of memory, so
small changes can be undone dozens of steps back (up to 96) and a whole-picture change once.

## How it works

`src/paint.h` has the memory map and the design. In short: the picture (`DOC`) is drawn into
with horizontal spans in a paint (two colours and an 8x8 pattern, a byte for each two pixels);
each frame the changed rectangle is copied or expanded onto the screen bitmap in every view,
at its zoom, and the pointer is drawn on top with what it covers saved. A preview (a
rubber-band shape) records the bytes it overwrites and is taken back from that list, or from
`BASE`, the picture before the operation. At the end of an operation the changed rectangle of
`BASE` goes into the undo log, and undo and redo swap a log record with the picture. A
floating selection is drawn over the picture by the renderer and only put into it when it is
put down.

`src/win.c` is the window manager. Each window's visible region (its rectangle less the
windows above it) is a list of rectangles, and everything drawn on the screen is clipped to
the region of what is being drawn, so a window can be drawn, or a view updated, in any order
without touching what covers it. A window being moved has what shows of it copied to where it
goes (except under the windows above it, which are never touched), and only the rest is
drawn: the parts the copy could not give and what it uncovers. Sizing draws the frame, the
new strips of the view and what the window uncovers. Picture windows sit at x = 3 (mod 4)
and are a multiple of 4 wide, so their views start on a 16-bit word of the screen and are
copied a word at a time.

Everything is drawn into the back one of two screens, and every drawing routine records the
rectangle it changed. At the start of the next vertical blank the back screen is shown (the
video reads `VID_BMP` on every line, so it is only written then); at the start of the next
frame the rectangles drawn in the last one are copied into the other screen, so the two stay
the same. The pointer is drawn into the back screen last and taken off it first, with what it
covers saved for each screen.

The ramsys T1S does about 3.75 MIPS, about 62,000 instructions a frame. `make test PERF=1`
prints the frames over that: strokes, previews and moving a selection mostly fit in one or
two; moving a palette takes two or three frames a step, moving or sizing a big picture
window about six; zooming a whole view, a large flood fill or a big filled shape take a few.

Memory: the program, data and stack below 0x01A000, the scratch area (the preview's list, the
fill's mask, or what an open menu covers), the picture at 0x021200, then the two screens, the
floating selection, the clipboard, `BASE` and the undo log (1.9 bitmaps) to the top of the
1 MB SRAM.

`fpga/lib/ps2.c` turns on the mouse's wheel (the IntelliMouse sample-rate sequence) and reports
it with `ps2_mouse_read()`; fsim's emulated mouse has the wheel too.
