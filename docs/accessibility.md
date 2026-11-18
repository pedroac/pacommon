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
5. **Keyboard only:** do every action without touching the mouse. The focus is always visible. A tool that is
   pointer-first on purpose (pacbar) says so in its `decisions.md`, with the keyboard path of each action it has
   in the rest of the desktop: keyboard completeness is checked for the desktop, and an exception has to be
   written down (principle 54).
6. **Pointer only:** do every action with the mouse alone. Nothing needs a hover or a time limit.
7. **Speech** (`SPEECH=speechd` or `atspi`): with `speech = yes`, close your eyes and do the
   actions. Focus, changes and messages are spoken; what is spoken is also on the screen. With
   `speech = no`, no backend is loaded: look at the process's memory and open files.
8. **Without speech:** build with `SPEECH=none` and check with `ldd` that no speech library is
   linked, and that keyboard and pointer still do everything.
9. **Braille** (tools that print, `BRAILLE=1`): print a page with a braille table and have a
   braille reader check it. With `BRAILLE=0`, liblouis is not linked.

## Known gaps (registered 2026-11-18)

What is not there yet for an accessible and friendly system, so that it is written down. **Read from the code and the
docs; nothing here was used by a person who relies on it.** Status is as of the date; each item that changes
gets a line in [decisions.md](decisions.md).

| # | Gap | Status |
| --- | --- | --- |
| 1 | **Input methods (IME).** A person who composes text (Chinese, Japanese, Korean, ...) could not type it in any pac field. | Client side done 2026-11-18 in pacmenu (search), pacgreet (username, layout search) and paclock (layout search), with `text-input-v3`; see decisions.md. **Not used with a real input method yet** (tests only). The compositor (pacland) does not relay text input to an input method yet: open, so it does not work on pacland until it does. |
| 2 | **The compositor (pacland):** support for a screen reader such as Orca, an on-screen keyboard. pacland has `contrast`, `text_scale`, `cursor_size`, `cursor_theme`, and (2026-11-18) `sticky_keys`, `slow_keys`, `bounce_keys` and a magnifier (`magnify` actions, bound by the person). | Open for the screen reader and the on-screen keyboard. The new ones are tested only with the headless compositor, not with a real keyboard, screen or person; the magnifier follows the pointer only, and may not apply to a fullscreen window (`../pacland/docs/decisions.md`). |
| 3 | **Speech needs an engine.** Named as a requirement in the docs (done 2026-11-18). No tool checks for one, and there is no sound cue when speech cannot be reached. | Requirement written; the check and the cue are open. |
| 4 | **Third-party apps:** a standard accessibility channel (AT-SPI, `SPEECH=atspi`) so that a screen reader can read other programs. | **Delayed** by the author (2026-11-18). Two halves, not started: the session and compositor side for third-party apps, and `SPEECH=atspi` for the tools. |
| 5 | **Reduced motion:** the shared `reduce_motion` setting (2026-11-18). | pacreact, the only tool with motion I found (its fade), obeys it. The other tools have none that I found (a text search, not a full read). pacland has no animation that a search of its source found (`animat|fade|slide|transition`). Not used by a person who needs it. |
| 6 | **Notifications that expire** (pacreact: 3 s, 6 s; critical ones never): whether that is an accepted exception to principle 35 is not written down. | Open. |
| 7 | **Uneven coverage:** pacbar has no text scale and no translations; pacpeek does not speak; right-to-left mirroring is only in pacmenu, pacgreet and paclock; contrast is tested for the shared palette and pacreact only (not pacpeek, pacland, pacbar). | Open, except: **pacbar does not need speech** (decided 2026-11-18). |
| 8 | **Missing apps:** a tray app (the one a keyboard-only person needs), a clock app, a network and volume app. | **Delayed** (decided 2026-11-18). |

Not known, and only people can say: whether any of it works for the people who rely on it (screen readers,
low vision, reading right to left, the machine-written translations); whether a newcomer can discover things
(a prompt at the start of a session is an idea, not designed); braille (principle 37) for the tools that print:
none exists.

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

**What it needs.** To be heard, speech needs more than the library: a **speech-dispatcher** server and a **speech engine** (a speech-dispatcher module) with a voice, such as **espeak-ng** (small, offline, many languages; an optional dependency of speech-dispatcher on Arch, where it is the usual choice), or Piper or Festival, and a sound server (PulseAudio or PipeWire). Without an engine the server has nothing to speak with. Packaging should make an engine a dependency of what it ships with speech on (the tools do not check for one: see the gaps below).

**On by default.** `speech` is `yes` unless a person says `no` (in the tool's file, or once in `pac.conf`).
It is on because the people who need it cannot set it first: at a login screen nothing can be configured
before speech is already needed. Where speech-dispatcher is not installed it does not work, and the tool
says so once and carries on in silence, so it costs a few resources and nothing else; for everyone who
does not want it, `speech = no` leaves the backend unloaded and unconnected, and `SPEECH=none` builds
without it. The author's decision (see [decisions.md](decisions.md), 2026-11-18). Not built: asking the
person, at the start of a session, whether they want it.

`speechd` talks to the speech-dispatcher server, so the tool says what it wants spoken: a small
dependency, and the right one when the tool knows what to say. `atspi` is the accessibility bus
that Orca reads: the tool would describe its interface to it, which is more work and more memory
but lets a screen reader that is already running read the tool. They are alternatives, and at
most one is built into a binary.
