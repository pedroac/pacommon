# Developing pacommon

## Build and test

```
make            # compiles each file with the hardening flags the tools use
make test       # builds and runs tests/test.c (no display, no network, no root needed)
make sanitize   # the same tests under AddressSanitizer and UBSan
make clean
tests/distros.sh   # make test (and sanitize) in containers of Arch, Debian, Ubuntu, Fedora,
                  # Alpine and openSUSE: needs podman or docker
```

There is no `make install`: nothing here is installed. `CC`, `CFLAGS`, `LDFLAGS` and `LDLIBS`
can be overridden as in the tools; hardening flags are added on top and cannot be dropped by
overriding `CFLAGS`.

The tests write only under a `mkdtemp` folder in `/tmp`, which they remove at the end. They change
`HOME`, `XDG_*` and the current folder for the duration of the run. The messages the code prints
about bad lines are silenced; only failures are printed.

The author uses Arch Linux, Hyprland and Sway, so a green `make test` there proves little about
other systems. Run `tests/distros.sh` before a release and record the result in
[platform.md](platform.md).

## Installing

`make install` leaves a tool working out of the box (principle 49): the program, the man page and
translations, and the configuration and assets it starts with, in the places it looks for them
(`/etc/pac/<tool>.conf`, `/etc/pac/pac.conf`, the PAM file of a locker, the session file of a
compositor, the service files). These files are the person's to edit, so:

- A file that **does not exist** is installed.
- One that **is the same** is left alone, quietly.
- One that **differs** is kept, with a warning that says how to replace it.
- `make install REPLACE=1` replaces it on purpose, and saves the old one as `NAME.bak.DATE`.
- `make uninstall` removes only files that are still the ones installed.

This lives in `pac.mk` (`pac_install_keep`, `pac_uninstall_keep`), so every tool does the same.
The installed `<tool>.conf` is the example with every line commented out, so it changes nothing.
What is not installed on its own is anything that changes how a person signs in: pacgreet's
greetd `config.toml` stays an example in the documentation folder. Try an install with
`make install DESTDIR=/tmp/root PREFIX=/usr`.

## Building

Every tool builds with `-Os`, each function and datum in its own section, `-Wl,--gc-sections` to
drop what nothing calls, `-Wl,--as-needed`, and `-s` to strip the symbol table (`make LDFLAGS=`
keeps it for a debugger; `CFLAGS` and `LDFLAGS` are the user's to override). The warnings are
`-Wall -Wextra -Wshadow -Wvla -Wformat-security`, and a warning is fixed, not switched off.
`pacommon` is compiled into each tool statically, from source (the translated tools pass
their own `i18n.h` to `config_file.c`, so a prebuilt library could not serve them all).

## Guards

The code reads from places that something else controls (principle 41), so each function that
takes input checks it before it trusts it:

- **Sizes:** compare with subtraction (`n >= MAX - len`), never `len + n >= MAX`, which can wrap.
  Every buffer has a named limit in the header, and what does not fit is refused or cut.
- **Pointers:** a parameter that a caller may pass as `NULL` is checked at the top.
- **Structures:** a structure with rules (`Field`: `count <= len`, the text ends at `len`) checks
  them on use and wipes itself if they are broken.
- **Files:** only regular files, of bounded size, opened so that they cannot block. A NUL byte in
  text is rejected, since every string function would stop there.
- **Printing:** text from a file or another program is shown with control characters replaced.
- **Memory:** every allocation is checked and has an owner that frees it; a function that fails
  after allocating frees what it allocated. Settings and anything else that is kept has a limit
  on how much it can grow.
- **Tests:** each guard has a test that fails without it, and `tests/test.c` feeds the parsers
  random input under `make sanitize`, which also finds leaks. `make` builds with `-Wvla`,
  `-Wformat=2` and `-Wshadow`.

## Performance hints

`compiler.h` has `PAC_LIKELY`, `PAC_UNLIKELY` and `PAC_COLD` (principle 9). The method:

1. **Find the time first.** Profile the real tool doing real work (`perf record`/`perf report`, or
   `perf stat` for counts), not a guess. Most of what looks hot is waiting for the compositor, a
   syscall or a file, which no hint helps.
2. **Change one thing, then measure again.** Same machine, same compiler, same input, the best of
   several runs. Both GCC and Clang, since they lay code out differently. Keep the hint only if
   the gain is larger than the spread between runs.
3. **Say what was measured** in a comment next to the hint, so that it can be checked and removed
   when the code changes.

Profile-guided optimization (`-fprofile-generate`, then `-fprofile-use`) does this for the whole
binary and needs no changes to the source. `CFLAGS` is the user's, so a packager can use it
without a Makefile change.

What was measured in pacommon on 2026-11-15 (Arch Linux, GCC and Clang, `-O2`): scanning 2,904
font files takes about 4 ms (`font_find`, mostly the kernel walking the folders), reading a
400-line settings file about 20 microseconds, and `field_append_key` about 9 nanoseconds. Putting
`__builtin_expect` on its full-field check changed the time by 1 to 2 percent, which is within
the spread between runs. So nothing in pacommon uses a hint. They are for the tools' own hot
loops: drawing, parsing `/proc`, decoding text.

## AI-assisted changes

AI is welcome here (principles 45 to 48): Claude generates, Dropstone reviews and improves, Gemini
and ChatGPT investigate. Before a change that an AI wrote is merged, the person merging it:

1. **Reads it all** and can say what each part is for. If a part is not simple enough to explain,
   it is rewritten.
2. **Checks it matches the surrounding code** (naming, comment density, idiom) and does no more
   than was asked: no extra options, no abstractions for one use.
3. **Checks what an AI investigation claimed** (a libc behaviour, a protocol detail) against the
   manual, the specification or a small experiment.
4. **Reads the tests** as well as running them: a test that cannot fail proves nothing.
5. **Runs the feature** on real setups, not only `make test`: `tests/distros.sh`, and a tool that
   uses the change on more than one compositor. Records the result in
   [platform.md](platform.md).

## Keeping the README right

The README is lean (principle 51): what it is, build, run, links. When a change touches an
option, a command, a file or a dependency, an AI checks the README against the code and the docs
and fixes what no longer matches (for example the file table, the build commands, the options and
their defaults), and a person adapts the wording so that it reads as written by a human. A tool
with a window has at least one screenshot, taken by a person, and retaken when the look changes.

## Rules for code in here

- It is compiled into every tool, so it has to be small, and what a tool does not call must not be
  linked in. No global constructors, and no file that makes a tool pull in a library it did not
  ask for. Dependencies: libc only.
- Fixed sizes before `malloc` (principle 6): a buffer whose size is known goes on the stack or is
  static, with its limit written down. Every `malloc` has a reason for not being a fixed size.
  When the heap is needed, allocate **once**, in one block sized from a measurement (run the
  biggest real input and read how much it used; write the number next to the limit). Measure with
  `mallinfo2()` before and after, or `/proc/self/status` (`VmRSS`), and put the result in
  [decisions.md](decisions.md).
- Constants a person may want to change go in `tunables.h` (principle 12), under `#ifndef`, with
  a comment, and a `_Static_assert` if a bad value would be unsafe. A literal size or default in
  the code is a sign that it belongs there.
- Rules serve people (principle 54): if the best choice breaks a rule, break it and write why in
  [decisions.md](decisions.md).
- Before adding a library to a tool, ask whether a program that is run could do the work
  (principle 3). pacreact's `rsvg-convert` and `pw-play` are the examples.
- A file here may need a library, but never imposes it: only the tools that list the file compile
  it, and a feature with a cost has a build switch (`textlayout.c` needs FreeType, and HarfBuzz and
  FriBidi unless built with `NO_I18N`; `textdraw.c` also needs Cairo). What belongs in a tool is what only that tool does, not what
  two tools do the same way. No Wayland code is written here yet, but it is not ruled out: the tools
  depend on Wayland by design (see the 2026-11-18 entry in [decisions.md](decisions.md)).
- Text is plain and says what to do (principle 32). Anything a person is told is also something a
  speech backend can say, so it is a string, not only a picture.
- User-visible messages go through `_()` (see `config_file.c` for how the tool's `i18n.h` is
  picked up).
- Anything that reads from a place the user or another program controls checks sizes, and bad
  input is reported once and skipped, never fatal. Secrets are wiped with `explicit_bzero`.
- Every function has a test, including the failure paths: the full buffer, the missing file, the
  malformed byte.
- Changing a header changes every tool. Check them all with `for d in ../pac*/; do make -C $d
  test; done`.

## Adding a file

1. Write `name.c` and `name.h`, with a comment in the header that says what the file is for and
   what a caller must not do.
2. Add it to `SRCS` in `Makefile`, tests to `tests/test.c`, a section to `docs/using.md` and a row
   to the table in `README.md`.
