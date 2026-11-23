# Consistency of the pac tools

An audit of the fourteen tools against pacommon, made on 2026-11-23 by reading the code and building
and testing every tool against the working copy of pacommon. It records what was done, and what is
left, in order of value. Update it when an item is closed.

## Done on 2026-11-23

| Change | Before | After |
| --- | --- | --- |
| Wayland protocol XML | `wlr-layer-shell` copied into 10 tools; `pac-window-workspaces`, `wlr-foreign-toplevel` into 3; `pac-drop-zones` into 2 (byte for byte the same) | One copy in `pacommon/protocol/`; each Makefile reads `$(PACOMMON)/protocol/`. `pacland` (the server) and its clients can no longer disagree about a pac protocol |
| `fcfont.[ch]` | `fonts.[ch]` in pacmenu, pacout, pacolor (three near-copies) | One, in pacommon |
| `icon_theme.[ch]` | `icons.[ch]` in pacmenu and pacout (identical but a word in a message) | One, in pacommon; `icons_init(theme_icons, prog)` names the tool; its limits are in pacommon's `tunables.h` (pacout builds with `-DICON_CACHE=64`) |
| `paths.[ch]` | pacmenu and pacout (pacout's was a subset) | One, in pacommon |
| `loop.[ch]` | pacreact and pacreactd (identical) | One, in pacommon |
| `cmdjob.[ch]` | `job.[ch]` in paconf, also used by pacpaper by reaching into `../paconf/src` | In pacommon (named `cmdjob` because pacgreet's `job.h` is something else) |
| pacpaper | Did not build: its Makefile needed `paconf/src/kit.c` and `win.c`, which had moved to pacommon's `gui_*` | Builds on pacommon only; no tool needs another tool's sources |
| Font lists | `PAC_FONTS_*` in `textdraw.c`, and a copy of the list in pacbar, pacpeek, pacgreet, paclock; pacbar and pacpeek lacked Atkinson Hyperlegible, which the docs say every tool prefers | `PAC_FONTS_REGULAR`, `_BOLD`, `_LIGHT` in `fontfind.c`; all five use them, so every tool now prefers Atkinson |
| Where pacommon is | `../pacommon` in five Makefiles, `./pacommon` in the others, three different error messages | The same four lines in every Makefile: `./pacommon`, else `../pacommon`, else one message |
| Style | Not written down | [style.md](style.md) |
| Static or shared library | Not possible | `make PACOMMON_LINK=static|shared`, evaluated in [library.md](library.md) (verdict: keep source) |

All fourteen tools build, and `make test` passes in all of them and in pacommon, with
`PACOMMON=../pacommon`.

## To do before this is visible: bump the submodules

Each tool has its own clone of pacommon in `./pacommon` (a submodule) and **those copies are on four
different commits** (`5ff6dcf`, `4d63dca`, `ae2d4ad`, `1b343ef`), none of them has this work, and
pacpeek has none (it uses `../pacommon`). Nothing was committed. To finish: commit pacommon, then in
each tool `git -C pacommon pull` (or `git submodule update --remote pacommon`) and commit the new
pointer. A tool with a stale submodule fails with "No such file: fcfont.c", which is the sign.

## Settings: who reads which shared key

A tool "has" a key if its code mentions it. A blank is a gap, not always a mistake (a tool with no
text has no `font`).

| Tool | contrast | text_scale | text_size_min/max | font | language | icons | speech | reduce_motion | resources | privacy |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| pacauth | x | x | x | x | x | | x | | | |
| pacbar | x | | | | | x | | | | |
| pacgreet | x | x | x | x | x | x | x | | x | x |
| pacland | x | x | | x | x | x | | | x | |
| paclock | x | x | x | x | x | | x | | x | x |
| pacmenu | x | x | | x | x | x | x | | x | x |
| pacolor | x | x | | x | x | | | | | |
| pacout | x | x | | x | x | x | x | | x | |
| pacpeek | x | x | | x | x | | | | | |
| pacreact | x | x | x | x | x | x | x | x | | |
| pacterm | x | x | | x | | | | x | | |

What the table says:

- `reduce_motion` is read by two tools (pacreact, pacterm); the others do not read it (whether each has motion to turn off was not checked).
- pacbar ignores `text_scale`, `font` and `language`; pacterm ignores `language`.
- `text_size_min/max` is read by four (pacauth, pacgreet, paclock, pacreact): in the others the person cannot bound the size.
- `theme` (color schemes in `themes/`) is documented, validated by `config_file.c` and read by nobody.
- Each tool parses these keys again in its own `settings.c` (pacgreet 141 lines, pacmenu 103, paclock 84,
  pacout 74, pacreactd 70, pacterm 59, pacolor 36): the same `contrast`, `text_scale`, `font`,
  `icons`, `resources`, `privacy` code, with small differences (pacmenu and pacout accept `icons = auto|theme|0`,
  pacgreet maps `theme` to "no icons because it has no theme").

## Left to do, by value

1. **A shared settings reader** (`pac_shared_settings.[ch]`): one struct and one handler for the eleven
   shared keys, with the rules of `docs/configuration.md` (what wins over what) written once and tested once.
   Each tool keeps its own keys. This removes the divergence above and the gaps follow from it. Not done: it
   touches every `settings.c` and needs each tool's tests, which is its own change.
2. **`reduce_motion` in pacgreet, paclock and pacmenu**, and `font`/`text_scale`/`language` in pacbar. These are
   accessibility settings that `pac.conf` promises for every tool.
3. **One window for pacauth.** pacauth's `kit.c`/`win.c` (283 + 749 lines) are an older fork of `gui_kit`/`gui_win`
   (455 + 981): the difference is an overlay layer with a dimmed screen (`win_set_scrim`), which `gui_win` lacks,
   and a few new features of `gui_*`. Add an overlay/scrim mode to `gui_win`, then delete the fork. Not done: it is
   the polkit prompt, and its window needs a hands-on check on a real session.
4. **`avatar.c`** exists four times (pacauth 181 lines, pacbar 175, pacgreet 196, paconf 323; 241 changed lines between
   the first two): load an AccountsService picture. Merge into `avatar.[ch]` in pacommon with paconf's as the base.
5. **The color decisions in [style.md](style.md)**: two background tones, pacbar's and pacpeek's and pacreact's own
   colors, `theme`.
6. **The icon theme lookup** is written four times: `icon_theme.c` (pacmenu, pacout), `theme_icon.c` (pacbar),
   `icon.c`/`sicon.c` (pacreact), and a fixed-path one in pacland (`bar.c`). Move them to `icon_theme.c` one at a time; pacreact and pacbar
   also draw SVG (by `rsvg-convert`), as `icon_theme.c` does.
7. **Smaller duplicates**: `i18n.h` exists in 4 identical copies and 7 variants; `display.c` (Wayland setup) in 7 tools,
   each 400 to 1,070 lines, all with their own Wayland registry code (not compared line by line); `shape.c` in pacpeek and pacreact;
   `util.c` in pacreact and pacreactd; `settings.c` shapes; the hardening flags and `build/pacommon_%.o` rules in
   fourteen Makefiles (`pac.mk` could hold them, with `PAC_HARDEN_CFLAGS`).
8. **pacode** is an empty folder.

## How the tools talk to each other

| Between | How | State |
| --- | --- | --- |
| pacreactd and pacreact, pacbar | A line protocol over a socket in `$XDG_RUNTIME_DIR`, parsed with pacommon's `daemon_value` | Good: one parser; the protocol is in `pacreactd/docs/protocol.md` |
| pacland and pacbar, pacout | `pac-window-workspaces`, `pac-drop-zones`, `wlr-foreign-toplevel` (Wayland) | Fixed today: one XML each, in `pacommon/protocol/` |
| pacland status | `$XDG_RUNTIME_DIR/pacland-SOCKET.state`, key=value lines | No other tool reads it (searched); they get windows from the Wayland protocols above |
| paconf and pacpaper | pacpaperconf is a window built on `gui_*` | Fixed today: no more source dependency on paconf |
| paconf and pacmenu | pacmenu's Settings category starts paconf's single-setting programs (pacmouse, pactablet, ...) | Works; they are built from paconf's one source tree |
| pacgreet and paclock | The same palette, fonts, layouts, `field`, announcer | Good: shared code; their own `announce.c` and `display.c` are separate (see 7) |
| Every tool and `pac.conf` | `config_file.c` | Good, except the gaps in the settings table |
