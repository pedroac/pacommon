# Principles of the pac tools

## In short

The pac tools are small, fast, Wayland-native programs for Linux, written in plain C, that anyone
can use and anyone can change.

- **Small and fast** (1 to 9 and 57). One coherent job each, few dependencies, little memory, no
  polling. Designed data-first: memory is on the stack or in one fixed block, and read in order.
  Speed work is measured. When smallness would cost reasonable convenience, convenience wins.
- **Easy to build, change and test, and Wayland native** (10 to 17). Optional features are compiled
  out, what a person may want to change is in one header, Linux first and tried on more than Arch,
  and Wayland first, through the protocols the compositor offers (X11 only as an option of the
  compositor, off the build when not wanted).
- **Configuration that just works** (18 to 25). Every setting has a default and no file is
  required. Files are plain `key = value`, easy to read and to create, documented, shared where
  the tools share a setting, and themes are the same kind of file.
- **For everyone** (26 to 38 and 58). Usable without setup, even by a child. The main task is large and
  the rest is quiet. What a person cannot see happen is confirmed in a line. Plain text, colors
  that a color-blind person can tell apart, keyboard and pointer both complete, speech and braille
  when wanted, translated. It detects what is probably wrong and remembers what you chose. What a
  person needs to see or hear is theirs to set, and no level overrides it.
- **Safe and respectful** (39 to 44). Hardened builds, guarded input, private files, nothing sent
  anywhere.
- **Made with AI, owned by people** (45 to 48). AI writes and reviews; a person reads, understands
  and tests every change, in different environments.
- **Open and well kept** (49 to 56). A plain build with no warnings, tests that need no
  display, a lean README, decisions and APIs written down, MIT licensed, nothing hidden, and
  contributions, forks and alternatives welcome.

What every pac tool (pacbar, pacgreet, paclock, pacland, pacpeek, pacreact, ...) should be. When a
tool breaks one of these, either fix the tool or change this file. A principle that cannot be
checked is a wish, so each one should say how it is checked: by a test, a measurement, or a person
following a step. Where it does not, the review in `AGENTS.md` is the check.

These are the default, not a law: a rule that stands in the way of the best or of a necessary
decision may be broken, on purpose and with the reason written down (principle 54). The one that
is never broken is *Respect the person* (principle 44).

**The numbers are stable.** Other projects refer to a principle by its number, so a number never
changes once this file is published: a new principle takes the next free number (and is placed
in the section it belongs to), and a principle that is dropped leaves its number unused. The
ranges in the summary above are today's snapshot, not a rule: a section may later hold
non-consecutive numbers.

`pacommon` is not a tool: it is code compiled into the tools, and it includes the GUI layer
(window, drawing, widgets) that the windowed tools share. The principles apply to it where they
make sense, and the notes marked **pacommon** say how.

## Small and fast

1. **No bloat.** Each tool or library does one coherent job. A feature that most people won't use
   is optional (principle 10) or left out.
2. **C, and libraries that earn their place.** Plain C, libc and Wayland, and the libraries listed
   in `docs/apis.md` (Cairo, FreeType, xkbcommon, libinput and sd-bus; PAM for paclock's login
   and libdbus for pacgreet's user picture; HarfBuzz, FriBidi and gettext for translated builds;
   the optional ones of principle 10). A library is welcome when it
   is lean and fits the job, even an unusual one: Clay for flexbox-style layout, or SDL3 for audio
   and input where it is smaller than the direct interface. A windowed tool still talks to the
   compositor through the Wayland protocols (principle 16), not through a windowing abstraction.
   What is not welcome is a heavy toolkit or runtime for what a few hundred lines do. A new
   dependency is chosen as principle 4 says, then added to `docs/apis.md` with the reason.
   **pacommon:** libc only.
3. **Delegate before depending.** A complicated or heavy feature is done by running an existing
   command-line program, not by linking its library or writing it again. pacreact draws SVG icons
   with `rsvg-convert` (linking librsvg pulled in GLib, libxml2 and ICU and more than doubled the
   daemon's resident size, for a few small icons drawn once) and plays sounds with `pw-play` or
   `paplay`. Use it for work that is occasional and whose output is small; a library is for what is
   called all the time. The helper is optional and can be named in the configuration (`player`). It
   is started without a shell, with fixed arguments of its own and any path from outside placed
   last (or after `--`), so the path cannot be read as an option; its stdin and stderr on
   `/dev/null`, a deadline and a cap on its output, and what it returns is checked like any other
   input (principle 41). Without it the feature is missing, said once (principle 43), and the tool
   carries on.
4. **Compare before choosing.** A library, framework or tool is chosen by comparing it with the
   alternatives, which include writing the code ourselves and delegating to a program (principle 3),
   on three things: **RAM** (the resident size of the idle tool), **CPU** (time spent and wakeups)
   and **UX** (what the person sees, feels and needs to do). Libraries that other programs on the
   machine already load share their code and are preferred when the rest is equal. The comparison is
   a short table in `docs/decisions.md` with numbers that were measured, or a note that they were
   not. A library's public API and ABI are a dependency for the projects that use it, so they are
   chosen and documented as this principle says. Check: every new dependency has a comparison table
   in `docs/decisions.md` (measured, or a note that it was not), and `docs/apis.md` links to it.
5. **Small footprint.** Low memory and a small binary, because these tools run all day on modest
   machines. `pacommon` is linked statically into each tool: its sources are compiled into each
   binary, not a shared library, so the linker keeps only what a tool calls. (`PACOMMON_LINK=static`
   or `shared` links `libpacommon` for the files that do not depend on the tool's configuration;
   the default stays, and why is in `docs/library.md`.) The GUI code is in
   `pacommon` too, so a tool that opens no window links none of it. A tool is
   built with `-Os`, each function in its own section, and `--gc-sections` to drop what nothing
   calls. Each tool's docs state the binary size and the resident memory of an idle tool, and its
   README links to them, so that growth is noticed.
6. **Stack before heap.** Memory whose size is known when the code is written lives on the stack or
   in static storage: no allocation to fail, nothing to free or leak, no fragmentation, and the
   footprint can be read from the source. Each such buffer has a limit that is written down in
   `tunables.h`, and what does not fit is refused or cut, never grown (principle 41). This is the
   preferred way. The heap is used when it is necessary or better: a size known only at run time,
   something large, or something that must outlive the function. There, **allocate once** is the
   default: a single block, made at start-up or at first use and sized by measuring the most the
   tool really needs (the number it was based on is written next to the limit), which the code then
   carves up itself (an arena, a pool, a ring) instead of many small `malloc`s that each cost a
   header and a chance to fail. Separate allocations are for what cannot be bounded. Every
   allocation is checked and freed, and wiped first if it held a secret.
   The stack is small, so a large buffer is static (static memory that is never touched is not
   resident), and no array or `alloca` is sized by input. Models: pacreact's history (a static ring
   of 64 fixed-size entries, about 60 KB, no `malloc`) and pacommon's settings (one 4 KiB block,
   where 32 settings took 96 allocations and 3 KB). Check: each `malloc` has a comment, or a line in
   `docs/decisions.md`, saying why one block of a known size would not do, and how much memory is
   needed.
7. **Data first, read in order.** A program is designed from its data: what it holds, how much of it
   there can be (principle 6), how it is laid out in memory, and how it flows from input to output.
   The code is the transformations of that data, written afterwards. Data that is used together is
   stored together, as an array of plain structs, or one array per field, and the loops walk it from
   the start to the end. Functions take the data they work on (an array and a count) rather than a
   web of objects that point at each other, and state is kept in one visible place, not hidden in
   the nodes of a structure. Prefer that to a linked list or a tree of separate allocations: the
   processor reads memory in lines and guesses what comes next, so a straight walk over contiguous
   data is much faster than chasing pointers, and it is smaller too. Keep what is read every time
   apart from what is read rarely (a name next to the numbers a loop needs makes every line carry
   the name). Records of a fixed size in a pool or a ring are the model: pacreact's history,
   pacommon's settings. This is for data that is large or walked often; for a dozen items, or
   something done once, the clearest code wins, and a speed claim is measured (principle 9). Check:
   a module says in its header what its data is and how big it can be, before it says what its
   functions do.
8. **Events, not loops.** Sleep until something happens (Wayland events, epoll, timers that are
   really needed). An idle tool uses no CPU. No polling to "stay up to date". Check: an idle tool
   wakes up zero times per second, or once per timer it has documented.
9. **Measure, then hint.** Speed work is done where a profile says the time goes, and kept only if
   the measurement shows it helped. In a hot loop or on an error path the compiler may be told which
   way a branch goes (`PAC_LIKELY`, `PAC_UNLIKELY`, `PAC_COLD` from `compiler.h`, which are
   `__builtin_expect` and `__attribute__((cold))`; the tools are built with GCC or Clang, so there
   are no fallbacks). A hint has a comment with what was measured, on which machine and with which
   compiler, and is removed when it no longer shows a difference. No hints "just in case", and
   clarity is not given up for a gain that cannot be measured. Check: each hint can be found in the
   source with its measurement beside it.
57. **Convenience before smallness.** Small and fast is the default, but not at the cost of
    reasonable convenience. When a smaller buffer, a shorter list, a missing feature or a faster
    path would make the tool harder to use, or make the person do work the tool could do, choose
    the convenient behavior. Convenience is the default; `resources = low`, or a build option that
    does the same, turns it off for someone who wants the smallest memory or the most speed.
    A comparison with another tool (its size, its memory, its speed) is a reference, to know where
    the tool stands and what its parts cost; it is not a contest, and no change is made only to
    lower a number. A tool for people is simple, and is not so minimal that it becomes
    inconvenient for the machine's sake.
    Convenience never excuses a missing check, a skipped event or an error that is not reported:
    those are correctness. Check: every convenience that costs memory or speed has a setting or a
    build option that removes it, and the default is the convenient one.

## Build options and tunables

10. **Optional means optional.** A feature that adds a dependency or a noticeable amount of memory
    (speech, braille, SVG, avatars, translation, ...) is chosen when building, with a Makefile
    variable such as `SPEECH=none`, `I18N=0`, `AVATAR=0` or `XWAYLAND=0`. Switched off, it is compiled out
    entirely: no code, no library, no linking. A build with every option off needs only the
    dependencies of principle 2. The build never turns a feature on because it happens to find a
    library: the same options give the same binary on every machine. The README lists each option
    with its default. Check: build with everything off, and see what `ldd` shows.
11. **Environment constants are build variables.** What differs between systems (`PREFIX`,
    `SYSCONFDIR`, `LOCALEDIR`, the path of a helper program, the default font) is a Makefile
    variable that reaches the code as a constant (`-D...` or a generated `config.h`), with a
    sensible Linux default. A packager changes a variable, not the source.
12. **Tunables live in one header.** What a person may want to change (the size of a buffer or a
    limit, a default value, a feature flag) is in one `tunables.h` per project, not scattered
    through the code. Each is under `#ifndef`, so that `-DNAME=value` overrides it without editing a
    file, and has a comment saying what it is, its unit and why that default. `_Static_assert`s
    refuse a value that would make the code unsafe. Check: a number that is a limit or a default is
    found in `tunables.h`; `grep` for a literal size in the code finds none that matters.

## Linux first

13. **Linux first.** The tools target Linux and use its interfaces directly (`/proc`, `/sys`, evdev,
    `memfd_create`, netlink) where that is smaller than a portability layer. No `#ifdef`s for other
    operating systems, no compatibility shims. Where a tool needs something Linux specific, its
    `docs/platform.md` lists it, so that someone who wants a port knows where to look.
14. **Say what was tested.** The project started on **Arch Linux** and was tested on **Hyprland**,
    **Sway** and **pacland**, the author's setup. Each tool's `docs/platform.md` names the
    distributions, compositors and versions it was really run on. Anything not listed is *untested*,
    not *supported*.
15. **Not only Arch.** Before a release, a tool is built and `make test` is run on other
    distributions: at least one glibc distribution that is not Arch (Debian, Ubuntu or Fedora) and
    one musl distribution (Alpine). Where possible the tool is also run on another wlroots-style
    compositor. Do not rely on Arch's layout: font and data paths, package names, a very recent
    compiler or libc. What breaks is fixed, or listed in `docs/platform.md` with the reason.
    `pacommon/tests/distros.sh` runs the pacommon tests that way.

## Wayland native

16. **Wayland first**, through the protocols compositors already offer (`wlr-layer-shell`,
    `ext-session-lock`, `xdg-shell`, ...). The tools are Wayland programs: none of them is an X11
    client and none has an X11 backend. They should work well with wlroots-style compositors and
    window managers such as Hyprland and sway, and pacland itself. A protocol a compositor lacks is
    a missing feature, handled as in principle 43, not a reason to crash. The one exception is
    compatibility in a compositor: pacland may run X11-only programs through Xwayland, because
    people still need some. It is an optional feature (principle 10): `XWAYLAND=1` by default so
    that those programs work, `XWAYLAND=0` for the leanest build, Wayland only, with no X11 code
    and no `DISPLAY`. It is a layer for running old programs, never a way to write a tool.
    Check: build with `XWAYLAND=0`, and `strings` finds no Xwayland code.
17. **Simple presentation.** A plain, readable canvas. Theming is a simple `.conf` file of values
    (principle 22), not an engine and not a CSS-like language. No animations for their own sake, no
    scripting layers.

## Configuration

18. **Works out of the box.** Every setting has a default, and a configuration file is never
    required: a person who has never seen a `.conf` can start the tool and use it (principle 26).
19. **One format, everywhere.** `<tool>.conf`: `key = value` lines, `#` starts a comment. Read from
    `/etc/pac/`, then `~/.config/pac/` (or `$XDG_CONFIG_HOME/pac/`), then the current folder, each
    overriding the earlier (`pacommon/config_file.h`). The settings that every tool shares
    (`contrast`, `text_scale`, `font`, ...) can be set once in `pac.conf`, in the same three places,
    which a tool reads first: what is in `<tool>.conf` beats it. Command-line options override the
    files. The files are the only source of settings: tools do not read environment variables for
    them. The only variables a tool reads are those that locate its files (`HOME`,
    `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_DATA_DIRS`, `XDG_CACHE_HOME`, `XDG_STATE_HOME`,
    `XDG_RUNTIME_DIR`), those that describe the session or a service (`WAYLAND_DISPLAY`, `DISPLAY`
    to detect an X11 session but not to use one, `XKB_DEFAULT_LAYOUT`, `GREETD_SOCK`, `SWAYSOCK`,
    `PULSE_SERVER`, `CUPS_SERVER`), `PATH` to find a helper program (principle 3), and the locale
    variables (`LANG`, `LANGUAGE`, `LC_*`). Reading another one means adding it to this list. A GUI
    library may use the same file for user preferences (speech, text scale, palette, icon sources,
    ...), but the application owns the file and passes the values to the library.
20. **Bad input is never fatal.** A wrong line or value is reported on stderr with file and line and
    skipped. The tool starts anyway. A message is printed once, not on every use.
21. **The same look, the same keys.** `contrast`, `icons`, `text_scale`, `font`, `language`,
    `speech` and `braille_table` mean the same in every tool that has them
    (`docs/configuration.md`). A tool that has a color scheme calls it `theme`.
22. **Themes are plain `.conf` files.** A tool that has themes keeps each one as a `<name>.conf` of
    `key = value` lines (colors, fonts, sizes), in the format of principle 19. They are found in
    `~/.config/pac/themes/`, then `/etc/pac/themes/`, then `$(PREFIX)/share/pac/themes/`, and chosen
    with `theme = name`. A theme sets only what it wants to change, and the rest keeps the default.
    No theme language, no code: a theme is a list of values, not a style sheet that matches
    elements. The name is a file name, never a path. A theme follows principle 33, and `contrast =
    high` wins over any theme.
23. **Easy to read, change and create.** Key names are whole words (`text_scale`, not `ts`), values
    are plain (`yes`/`no`, a number, a name), one setting per line. Each tool ships an example
    `<tool>.conf` that lists every option, commented out at its default value with a line saying
    what it does, so that a file is made by copying it and removing the `#` from the lines to
    change. `make install` puts it in `$(PREFIX)/share/doc/<tool>/`.
24. **Every option is documented.** The example `<tool>.conf` is the reference: what the option
    does, its values and its default. A test reads it back against the options the code accepts, so
    that an option cannot be added, removed or renamed without it.
25. **Options for footprint, speed and accessibility.** What a build can remove (principle 10), the
    configuration can also switch off while it is still compiled in. Features that cost memory or
    CPU can be turned off (`resources`, `animations`, ...), and the accessibility features of
    principles 34 to 37 can be turned on or off (`contrast`, `text_scale`, `speech`, ...). Off means
    off: nothing is loaded, opened or connected for it. The convenient behavior is the default
    (principle 57).

## People

26. **Usable by anyone.** The people who use a graphical tool include children and people who have
    never configured anything. Starting it, and what it does first, need no setup, no manual and no
    jargon. Targets are large and labels are words. An action that is hard to undo asks twice
    (pacgreet's restart and power off). Check: someone who has not seen the tool uses it without
    being told what to do.
27. **The main task comes first.** The thing a person does every time is the biggest and the most
    central; what they sometimes do is smaller; what they rarely do is smaller and quieter still. In
    pacgreet the user name and the password are very large, the session and the keyboard layout are
    smaller and dimmer, in that order of importance, and restart and power off are smallest. Size,
    position and contrast carry the order, not decoration. Check: someone looking at the screen for
    a second knows where to start.
28. **Show only what is needed.** Information comes in layers: what the person needs now is shown;
    the rest is one step away, not on the screen already. No wall of text, no long list of options
    at first sight, no message that needs more than a line or two. When a choice has a good default,
    it is made, not asked. Check: a screen shows one thing to do, and everything else on it is
    smaller.
29. **Confirm what is not obvious.** When a person does something whose effect they cannot see, the
    tool says briefly that it happened, and says plainly if it did not. Reloading the configuration
    of pacland is the model: it is rare, it is done by a key, and nothing on the screen changes if
    nothing in the file did, so a short notification ("Configuration reloaded", or "Configuration
    not loaded: line 12 is wrong; the old one is kept") tells them it worked. The same for saving,
    applying a setting in the background, and a command sent from a keybind. Do not confirm what is
    already visible (a window that opens, a volume bar that moves): that is noise (principle 28).
    The message is short, goes through gettext (principle 38), and is also spoken when speech is on
    (principle 36).
30. **Fix what is probably wrong.** When something is likely not to work, such as the wrong keyboard
    layout or language, the tool works out the right value from what the system says (pacgreet reads
    `/etc/X11/xorg.conf.d`, `/etc/default/keyboard` and `/etc/vconsole.conf`, because some
    compositors ignore them) and uses it. It shows what it chose, lets the person change it in one
    step in the interface, and also in the settings file for whoever prefers that. The change is
    kept (principle 31).
31. **Remember for convenience.** What a person chose last time (the user, the session, the layout)
    is remembered, as pacgreet does, so that the next start needs fewer steps. It is a small text
    file in the user's own state or cache directory, private (principle 42), of bounded size, and
    never holds a secret. Each tool's README says exactly what it keeps, and a setting such as
    `remember_user = no`, or deleting the file, forgets it (principle 44).
32. **Easy to read.** A plain sans-serif font and a comfortable default size, adjustable with
    `text_scale` (0.5 to 2.5). Short sentences in plain language. A message says what happened and
    what to do about it; no jargon and no abbreviations that save only a few letters. Text sits on a
    plain background. Check: read every message of the tool aloud once.
33. **Colors that are pleasant, mean the same thing, and can be told apart.** A calm, restrained
    palette. Meanings follow the common ones: red for errors and danger, green for success, amber
    for warnings, blue for information and selection. People who are color blind must be able to
    tell the colors apart: colors that mean different things also differ in lightness, never in red
    against green alone, and are chosen from a color-blind-safe palette such as Okabe-Ito. Nothing
    depends on color alone: every meaning also has a word, an icon or a shape. Text has a contrast
    ratio of at least 4.5:1 against its background, and graphics at least 3:1. Check: a test
    computes the contrast of each palette pair and the distance between the meaning colors as seen
    with protanopia, deuteranopia and tritanopia.
34. **Accessible by default.** Legible defaults, a text scale, and a high-contrast mode (black and
    white, nothing dimmed) one setting away, with nothing to install first.
35. **Keyboard and pointer, both complete.** Every action can be done with the keyboard alone and
    with the pointer alone. Neither is an afterthought: no feature that only a key or only a click
    reaches, nothing that needs hovering or a time limit. The keyboard focus is always visible.
    Check: use the tool once without touching the mouse, and once without the keyboard.
36. **Speech, optional.** A graphical tool can speak what a person who cannot see the screen needs
    to know: what has focus, what changed, every message and error. Built with `SPEECH=speechd`
    (libspeechd) or `SPEECH=atspi` (libatspi); `SPEECH=none` compiles it out, and that is the build
    for someone who does not need it. At most one backend is built. In the configuration, `speech =
    no` leaves the backend unloaded and unconnected. Everything spoken is also shown as text. A tool
    built without speech still works fully with keyboard and pointer. The GUI part of `pacommon`
    provides this layer (accessible roles, names and states, and the speech backend) so that every
    tool using it gets it; the tool then configures it.
37. **Braille, optional.** A tool that prints builds with `BRAILLE=1` to translate text to braille
    with liblouis, and the configuration says which table (`braille_table`). `BRAILLE=0` compiles it
    out and links nothing. The GUI part of `pacommon` provides the braille layer in the same way.
38. **Translated.** All text a person sees or hears goes through gettext with English as the
    fallback; catalogs live in `po/`. A machine-written translation is marked `fuzzy` in its `.po`
    entry, and the catalog header says so, until a native speaker reviews it. A tool built with
    `I18N=0` is English only and links no translation code.
    Text of every script is drawn right: shaped, in the right order, and cut and wrapped where the script allows it
    (`textlayout.[ch]`, HarfBuzz and FriBidi in translated builds); an English-only build draws left to right.

58. **People set the limits, levels set the defaults.** What a person needs to see, hear or read
    (`contrast`, `speech`, `text_scale`, `text_size_min`, `text_size_max`) is theirs, and no level
    of `resources` or `privacy` turns it off or overrides it. A level changes what a tool does by
    default, in ordered steps (three for `resources`, four for `privacy`) whose meaning each tool
    writes down; it never takes away a result a person needs to be told. The size limits hold
    whatever a tool's base sizes are, except that an absurd limit is pulled to a sane one and a card
    may stay on the screen (`docs/configuration.md`). **Privacy prevails over resources.** A person
    who sets a higher privacy level has a reason (a company policy, a shared screen), so a tool
    gives up nothing of what that level promises to save memory, and uses the memory, the
    processes and the time it needs for it, even above what `resources` would allow. Check: the
    tests of `pac_length_clamp` and of the shared keys; each tool's README lists what its levels
    do, and says what a privacy level costs under `resources = minimal`.

## Safe, and respectful

39. **Hardened builds.** PIE, RELRO, `-z now`, no executable stack, stack protector, stack clash
    protection, `_FORTIFY_SOURCE=3`, CET where the compiler offers it. These are added by the
    Makefile and cannot be dropped by overriding `CFLAGS`.
40. **Fail closed, least privilege.** A locker stays locked if it crashes. Secrets are wiped with
    `explicit_bzero` after use and kept out of core dumps. Code that runs before login drops what it
    doesn't need.
41. **Distrust files and input.** Files that can run commands are used only if the user (or root)
    owns them and nobody else can write to them. Size limits and checks on anything read from a
    place the user or another program controls: files, sockets, Wayland events, command-line
    arguments, environment. A bounded buffer is never trusted to be terminated, and an allocation is
    never trusted to succeed. Concretely: a file that is read must be a regular file (a pipe blocks,
    a device never ends) of a bounded size; a length is checked before it is used to copy, so that
    the check cannot itself overflow (`len <= MAX && n >= MAX - len`, not `len + n >= MAX`); an
    index or count that a caller could have corrupted is checked, and a structure that breaks its
    own rules is wiped, not trusted; text from outside is cleaned of control characters before it
    is printed; an environment path that is not absolute is ignored; a pointer a caller may pass
    as NULL is checked. Every limit is a named constant in the header. Check: the tests in
    `tests/test.c` feed NUL bytes, a pipe, a device, an oversized file and random input to the
    parser; a new guard gets a test that fails without it.
42. **Private files.** Files and folders that a tool creates in a person's own directory (settings
    it writes, remembered choices, caches, sockets, backups) can be read and written only by that
    person: files with mode 0600 and folders with 0700, given explicitly when they are created
    (never left to the umask), made with `open(O_CREAT|O_EXCL|O_NOFOLLOW)` or `mkdir` and not
    through a symbolic link, and written to a temporary file in the same folder and renamed into
    place, so that a crash never leaves half a file. A file that is read back from there must be
    owned by the person and not writable by anyone else, or it is not trusted (principle 41).
    Runtime files go in `$XDG_RUNTIME_DIR`. System-wide files that `make install` puts in `/etc` are
    readable by all (0644) unless they hold a secret, and then they are not. Check: a test creates
    the file under a permissive umask and finds mode 0600; a test that stops the writer before the
    rename finds the old file intact; a file owned by another user, or writable by others, is
    refused.
43. **Degrade, don't break.** A missing font, icon or helper program is said once on stderr and the
    tool carries on with a fallback. A speech backend that is not running is the same: said once,
    and the tool keeps working.
44. **Respect the person.** A tool does what it says and nothing else. It sends nothing anywhere: no
    telemetry, no accounts, no update checks, no advertising. It does not trick or hurry anyone: no
    dark patterns, no pressure, no hidden costs. What it stores is minimal, local, listed in its
    README and easy to erase. Its messages are honest about what happened and what it could not do.
    This is the one principle that is never traded away. Check: `strace -f -e trace=network` on an
    idle tool shows no network syscalls; the README lists every file the tool stores, and deleting
    them leaves it working.

## AI-assisted, human-owned

45. **AI is welcome.** Using AI to write code is encouraged. The roles today: **Claude** generates
    code, **Dropstone** reviews and improves it, **Gemini** and **ChatGPT** are used for
    investigation (how a protocol or API works, what causes a bug, what the options are). The tools
    may change; the three roles (generate, review, investigate) stay. Each README's *AI Assistance
    Disclosure* says which tools were used for what, and each project has an `AGENTS.md` that tells
    an AI agent how to work in it.
46. **Simple and readable first.** Generated code is accepted only if a person can read it and
    explain it. It is no bigger than the job, has no cleverness and no speculative options, and
    reads like the code around it (naming, comment density, idiom). Principles 1 to 5 apply to it
    exactly as to hand-written code. Code that is not simple is rewritten or deleted, however well
    it works.
47. **A human owns every line.** A person reads and understands each change before it is merged. An
    AI review (Dropstone) adds to that and never replaces it: it is asked to judge simplicity, code
    quality, security and bugs (the questions are in `AGENTS.md`). What an AI investigation says is
    a lead, not a fact: it is checked against the documentation, the protocol XML, the source or an
    experiment before the code relies on it.
48. **A human tests every feature, in different environments.** Unit tests and AI-written tests are
    necessary, not sufficient. A person runs each feature on real setups: the distributions of
    principle 15 and more than one compositor, and for accessibility with the keyboard alone, the
    pointer alone, and a screen reader or speech. AI-written tests are read too, to check that they
    assert the right thing. What was tried, where, and by whom goes in `docs/platform.md` (principle
    14).

## Project hygiene

49. **Plain build.** A `Makefile` with `PREFIX` and `DESTDIR`, no autotools. `make` builds, `make
    test` tests, `make install` installs, `make clean` cleans. Shared code comes from `pacommon`,
    which is cloned next to the tool (`../pacommon`, or wherever `PACOMMON=` says) and gives the
    Makefiles their install rules (`pac.mk`); a tool's README says how to get it. The build has no
    warnings: they are fixed, not switched off. `make install` leaves the tool working out of the
    box: it installs the program and the configuration and assets it starts with. A file that is
    already there is the person's: it is kept, with a warning, and `make install REPLACE=1` replaces
    it on purpose, saving the old one as `NAME.bak.DATE`. **pacommon:** nothing is installed by the
    tools' `make install`; its own `make install-lib` installs `libpacommon` and its headers only for
    someone who chose `PACOMMON_LINK=shared` or `static` (`docs/library.md`).
50. **Tested without a display.** Logic that does not need Wayland (parsers, layout, text, colors,
    state) has unit tests run by `make test`, including the failure paths: the missing file, the
    full buffer, the malformed byte. A bug that is fixed gets a test that fails without the fix. A
    parser is also fed random and hostile input (NUL bytes, control characters, lines and files over
    the limit, a pipe where a file should be) under AddressSanitizer and UBSan, which also report
    leaks (`make sanitize` where there is one). They run before a release, and `make test` builds
    with warnings that catch variable-length arrays and unsafe formats.
51. **A lean README.** What it is, build, run, a link to everything else. A screen or two, not a
    manual: details live in `docs/` and the README links to them. A tool with a window has at least
    one screenshot, taken by a person (`pacommon` is not visual, so it has none). An AI may draft
    the README and keeps it consistent with the code and the docs on every change (the options of
    principle 10, the keys of the example `.conf`, the commands), and a person adapts it, so that it
    reads as written by someone.
52. **Documented, and decisions written down.** A `docs/` folder where there is more to say, and a
    man page for tools that are run from a shell. `docs/decisions.md` records each implementation
    decision that is not obvious from the code: what was decided, why, what else was considered, and
    when. A reader should not have to guess why a dependency, a default or a limit is there.
    **pacommon:** its `docs/` says how a tool uses each file and what a change to it must keep true.
53. **APIs are written down in one place.** `docs/apis.md` in pacommon lists every library and
    protocol the tools use: where its documentation and its headers are, which version was tested,
    and where a tool sets that version (pacland sets the wlroots version once, in its `Makefile`).
    wlroots has no stable API, so it matters most there, but every API is listed. An AI agent reads
    that file before it writes code against an API, checks it against the version that is installed,
    and updates the file when an API changes or a version is moved. A library's own public API and
    ABI are written down in the same way, because they are what the projects that use it depend on.
    Check: every external library in a Makefile's `PKGS` or `LDLIBS` has a line in `docs/apis.md`
    (libc, libm and libpthread are not listed).
54. **Rules serve people.** These principles are the default, not a law. When the best or a
    necessary decision means breaking one, break it on purpose: write what was decided, why, and
    what was weighed in `docs/decisions.md`, and keep the rest. Principle 44 is not one of the ones
    that bend.
55. **MIT licensed**, with a `LICENSE` file in every project, and an *AI Assistance Disclosure*
    section in every README saying how AI tools were used and that a human reviewed and tested the
    code (principles 45 to 48).
56. **Open and transparent.** The code is open and free to use, change and fork, and contributions
    are welcome (`CONTRIBUTING.md`). Forks and alternatives written for other needs, or from
    scratch, are as welcome as contributions. A person who prefers not to use AI-written code can
    still use the apps and their documentation as a reference, and `docs/decisions.md` says why
    things are the way they are, so that an alternative does not start from nothing. How AI was used
    is stated plainly, and what has and has not been tested is too (principle 14). Nothing is hidden
    to look better than it is.

## Related quotations

These say in other words what the principles above say. They are reminders, not rules.

> "The Sabbath was made for man, not man for the Sabbath." — Jesus
>
> Rules serve people, not the other way around (principle 54).

> "It is not that we have a short time to live, but that we waste a lot of it." — Seneca
>
> An idle tool uses no CPU, and work is measured before it is optimized (principles 8 and 9).

> "What Andy giveth, Bill taketh away." — industry saying
>
> Speed won in one place is lost in another; the whole tool is what matters (principles 1 and 5).

> "We have to stop conflating 'easy' with 'simple'. Pulling in a massive library is easy, so we do
> it, and we end up with a complected, bloated mess." — Rich Hickey
>
> Easy for the person writing the code is not simple for the person reading it, and neither is the
> same as convenient for the person using it (principles 2, 4, 46 and 57).

> "A little copying is better than a little dependency." — Rob Pike
>
> A small piece of code we own can be better than a dependency we do not (principles 2, 3 and 4).
