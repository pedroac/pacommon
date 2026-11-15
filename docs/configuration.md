# Configuration

How every pac tool is configured (principles 18 to 25). Each tool's own example `<tool>.conf` is
the reference for its options; this page is for what they have in common.

A GUI library may use the same file for user preferences (speech, text scale, palette, icon
sources), but the application owns the file and passes the values to the library.

## The file

`<tool>.conf`, `key = value` lines, `#` starts a comment. Read from `/etc/pac/`, then
`~/.config/pac/` (or `$XDG_CONFIG_HOME/pac/`), then the current folder; each overrides the earlier,
and command-line options override all of them. Settings come from these files only, never from
environment variables. A wrong line is reported with its file and line and skipped. A line is at
most 1023 bytes and has no NUL byte; the file is a regular file of at most 64 KiB. How a tool reads
it: [using.md](using.md).

To make a file from scratch, copy the tool's example `<tool>.conf` (installed in
`$(PREFIX)/share/doc/<tool>/`), and remove the `#` from the lines to change. Every option is in it,
at its default, with a line saying what it does.

```
# How big text is drawn, from 0.5 to 2.5.
#text_scale = 1.0
```

## pac.conf: what every tool shares

The settings in the table below can be set once, for all the tools, in `pac.conf`, found in the
same three places as the tools' own files. A tool reads it first, then its own `<tool>.conf`, so
**a setting in `<tool>.conf` beats the same one in `pac.conf`**, wherever each file is. A tool
that does not have a setting (`speech` in a tool that cannot speak) ignores it without a warning.
A key that is not a shared one, or a value that cannot be right (`text_scale = big`), is reported
with its file and line and skipped. `make install` installs a `pac.conf` with every line
commented out, so it changes nothing until a line is edited. A tool opts in by using
`pac_config_load` (or `pac_config_load_shared`, for one that reads its files itself): see
[using.md](using.md).

## Themes

A theme is a `<name>.conf` file in the same format, with colors, fonts and sizes, and nothing
else: no code. It sets only what it wants to change. Tools look for it in
`~/.config/pac/themes/`, then `/etc/pac/themes/`, then `$(PREFIX)/share/pac/themes/`, and
`theme = name` chooses it (the name is a file name, never a path). `contrast = high` wins over any
theme, and a theme has to meet the color rules of principle 33. To make one, copy an existing
theme, rename it and change the values.

## What a tool remembers

A graphical tool may remember what a person chose last time (principle 31) so that the next start
is quicker: pacgreet remembers the user, the session and the keyboard layout. It is a small text
file in the user's own cache or state directory, mode 0600, never a password, and the tool's
README says what it holds. To forget it, delete the file or set the option the README names (for
pacgreet, `remember_user = no`).

## Keys that mean the same everywhere

A tool that has the feature uses these names and values. The last three are not yet used by any
tool.

| Key | Values | Default | Meaning |
| --- | --- | --- | --- |
| `contrast` | `normal`, `high` | `normal` | High contrast: black and white, nothing dimmed |
| `text_scale` | 0.5 to 2.5 | `1.0` | Size of all text, as a multiple of the default |
| `font` | a font name | the tool's own | The font for text |
| `language` | a locale such as `pt_BR` | from `LANG` | Language of messages |
| `icons` | `auto`, `theme`, `0` | the tool's own | The icon set |
| `theme` | a name | the tool's own | The color scheme, for tools that have more than one |
| `speech` | `yes`, `no` | `yes` (when built with speech) | Speak focus, changes and messages |
| `low_memory` | `yes`, `no` | `no` | Trade speed or looks for less memory: no caches, no extras |
| `braille_table` | a liblouis table name | from `language` | Braille table, for tools that print |

`speech` and `braille_table` do nothing in a build without `SPEECH` or `BRAILLE`, and nothing in a
tool that does not have the feature: the option is accepted and ignored silently, so that one file
works with every build. No tool implements `low_memory` yet: the build options below are what
remove the optional features today, and a tool that adds a runtime trade-off reads it.

The convenient behavior is the default; a tool with a runtime trade-off reads `low_memory = yes` to
turn off what costs memory or speed (principle 57).

## Build options

What a build can remove, so that a tool needs fewer libraries and less memory. Principle 10: a
variable, `0`/`none` compiles the feature out, nothing is detected automatically.

| Variable | Values | Effect |
| --- | --- | --- |
| `SPEECH` | `none`, `speechd` (default), `atspi` | Speech through libspeechd or libatspi, or none. `speech.c` and `pac.mk` implement `none` and `speechd`; `atspi` is refused until it exists |
| `BRAILLE` | `0` (default), `1` | Braille output with liblouis, for tools that print |
| `I18N` | `0`, `1` | Translated messages (gettext, HarfBuzz, FriBidi) |
| `PREFIX`, `SYSCONFDIR`, `LOCALEDIR` | paths | Where things are installed and looked for |

Each tool adds its own (`AVATAR`, `NO_SVG`, ...), listed in its README. Why `SPEECH` is on by
default and `BRAILLE` is not: [decisions.md](decisions.md).
