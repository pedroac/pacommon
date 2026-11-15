# Accessibility

What principles 32 to 37 ask of every tool, as something to check before a release. Record what
was checked, where and by whom in the tool's `docs/platform.md`.

## Checklist

1. **Read** every message of the tool aloud. Does it say what happened and what to do? Is there a
   word that could be plainer?
2. **Text size:** run with `text_scale = 0.5` and `2.5`. Nothing is cut off or overlaps.
3. **Contrast:** run with `contrast = high`. Everything is still there and still readable, and
   nothing is dimmed. A test checks the 4.5:1 text and 3:1 graphics ratios of the palette.
4. **Color blindness:** look at the tool through a simulation of protanopia, deuteranopia and
   tritanopia (the colors are also checked by a test). Every state can still be told apart, and
   every state also has a word, an icon or a shape.
5. **Keyboard only:** do every action without touching the mouse. The focus is always visible.
6. **Pointer only:** do every action with the mouse alone. Nothing needs a hover or a time limit.
7. **Speech** (`SPEECH=speechd` or `atspi`): with `speech = yes`, close your eyes and do the
   actions. Focus, changes and messages are spoken; what is spoken is also on the screen. With
   `speech = no`, no backend is loaded: look at the process's memory and open files.
8. **Without speech:** build with `SPEECH=none` and check with `ldd` that no speech library is
   linked, and that keyboard and pointer still do everything.
9. **Braille** (tools that print, `BRAILLE=1`): print a page with a braille table and have a
   braille reader check it. With `BRAILLE=0`, liblouis is not linked.

## Colors

The meanings, as in principle 33: red for errors and danger, green for success, amber for
warnings, blue for information and selection. The starting point for the palette is Okabe-Ito,
which was designed to be told apart by all kinds of color blindness. The palette lives in one
place in `pacommon` (`palette.[ch]`) so that a tool can match the others (pacgreet and paclock use
it; pacpeek, pacreact and pacland still have their own colors); its contrast and color-blindness
checks are in `tests/test.c`.

## Speech

The shared code is `speech.[ch]` (see [using.md](using.md#speech-speaking-what-the-screen-shows)); it implements `speechd`. The tool decides what is spoken.
It has been tested with speech-dispatcher (see [platform.md](platform.md#what-has-been-tested)).

`speechd` talks to the speech-dispatcher server, so the tool says what it wants spoken: a small
dependency, and the right one when the tool knows what to say. `atspi` is the accessibility bus
that Orca reads: the tool would describe its interface to it, which is more work and more memory
but lets a screen reader that is already running read the tool. They are alternatives, and at
most one is built into a binary.
