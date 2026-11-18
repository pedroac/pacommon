# APIs

Every library, protocol and program the pac tools use, with where its documentation is, which
version was tested, and where a tool sets it (principle 53). **AI agents: read this file before
writing code against any of these, check the headers or the manual of the version that is
installed (`pkg-config --modversion NAME`), and update this file when an API changes or a
version is moved.** Do not write an API call from memory: wlroots in particular changes between
releases.

Versions are those installed on the author's Arch Linux on 2026-11-15, where they were checked.
"Not checked" means the entry was not looked at against an installed version.

## wlroots (unstable)

wlroots has **no stable API**: functions, structs and signals are renamed and removed between
releases, so code that builds against 0.20 may not build against 0.21.

| | |
| --- | --- |
| Used by | pacland (the compositor) |
| Tested | wlroots 0.20.2 (Arch package `wlroots0.20`) |
| Where the version is set | `WLROOTS ?= wlroots-0.20` in pacland's `Makefile`: the one place that names it |
| Source and releases | https://gitlab.freedesktop.org/wlroots/wlroots and `/-/releases` (the release notes list every breaking change) |
| Headers (the real reference) | https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/include/wlr, or `/usr/include/wlroots-0.20/wlr/` for the installed version |
| Protocols it adds | https://gitlab.freedesktop.org/wlroots/wlr-protocols (`wlr-layer-shell`, `wlr-output-management`, ...) |

To move to a release: change `WLROOTS`, read that release's notes, build, fix what no longer
compiles, update the version above, and add a line to [decisions.md](decisions.md). pacland's
README describes which of its files include wlroots, so the change stays in those.

## Wayland and its protocols

| | |
| --- | --- |
| Used by | all the graphical tools |
| Tested | wayland 1.26.0, wayland-protocols 1.49 |
| Core library | https://wayland.freedesktop.org/docs/html/ |
| Protocol browser (every protocol, with its status and which compositors support it) | https://wayland.app/protocols/ |
| `text-input-v3` (input methods; `src/textinput_wayland.c`) | https://wayland.app/protocols/text-input-unstable-v3 and `text-input-unstable-v3.xml` in `wayland-protocols` |
| Standard protocols (`xdg-shell`, `viewporter`, `fractional-scale`, `ext-session-lock`, ...) | https://gitlab.freedesktop.org/wayland/wayland-protocols |
| wlroots protocols (`wlr-layer-shell`) | https://gitlab.freedesktop.org/wlroots/wlr-protocols |

`text-input-v3`: the installed XML is interface version 2, the tools bind version 1 (the one every compositor with it has). The
client only enables, disables, sets the content type and the cursor rectangle and commits, and takes `preedit_string`,
`commit_string`, `delete_surrounding_text` and `done` (applied in that order, on `done`).

A tool keeps the XML of the protocols it uses in its `protocol/` folder or reads it from
`wayland-protocols`; `wayland-scanner` makes the code. Check on wayland.app that a protocol is
stable (or staging) and which compositors have it before depending on it.

## Libraries linked into the tools

| Library | Used for | Tested | Documentation |
| --- | --- | --- | --- |
| Cairo | drawing; `textdraw.c` uses `cairo_ft_font_face_create_for_ft_face` and `cairo_font_face_set_user_data` (FreeType's `FT_Done_Face` as the destroy function: the pattern the Cairo manual gives for `cairo-ft.h`), `cairo_get_font_face`, `cairo_show_glyphs` and `cairo_glyph_path`. Cairo's cache of scaled fonts holds the faces, so `cairo_debug_reset_static_data` must not be used where they are in use | 1.18.6 | https://www.cairographics.org/manual/ (`cairo-ft.h`) |
| FreeType | font files | 2.15.0 | https://freetype.org/freetype2/docs/reference/index.html |
| HarfBuzz | shaping text (translated builds): `textlayout.c` uses `hb_ft_face_create_referenced`, `hb_buffer_*`, `hb_shape` and `hb_unicode_general_category` / `hb_unicode_script` | 14.6.0 | https://harfbuzz.github.io/ (`/usr/include/harfbuzz/hb-ft.h`) |
| FriBidi | right-to-left text (translated builds): `textlayout.c` uses `fribidi_get_bidi_types`, `fribidi_get_bracket_types`, `fribidi_get_par_direction`, `fribidi_get_par_embedding_levels_ex` | 1.0.17 | https://github.com/fribidi/fribidi (`/usr/include/fribidi/fribidi-bidi.h`) |
| xkbcommon | keyboard layouts | not checked | https://xkbcommon.org/doc/current/ |
| xkeyboard-config | `layout.c` reads its rules list (`/usr/share/X11/xkb/rules/evdev.lst`, or under `xkeyboard-config-2`) and its translations (gettext domains `xkeyboard-config-2` and `xkeyboard-config`) | 2.48 | https://gitlab.freedesktop.org/xkeyboard-config/xkeyboard-config |
| PAM (libpam) | paclock's login | 1.7.3 | `/usr/include/security/pam_appl.h`, `man pam_start`, https://linux-pam.org/ |
| libinput | pacland's input | not checked | https://wayland.freedesktop.org/libinput/doc/latest/ |
| sd-bus (libsystemd) | notifications over D-Bus | systemd-libs 262.1 | https://www.freedesktop.org/software/systemd/man/latest/sd-bus.html |
| libdbus (dbus-1) | pacgreet's user picture | 1.16.2 | `/usr/include/dbus-1.0/dbus/dbus.h`, https://dbus.freedesktop.org/doc/api/ |
| gettext | translated text | not checked | https://www.gnu.org/software/gettext/manual/ |

These predate principle 4, so their comparison is not recorded; a new library gets a table in
[decisions.md](decisions.md) and a link here.

## Optional, chosen when building

| Library | Option | Documentation |
| --- | --- | --- |
| speech-dispatcher (libspeechd) | `SPEECH=speechd` | https://freebsoft.org/doc/speechd/ and https://github.com/brailcom/speechd |
| AT-SPI (libatspi) | `SPEECH=atspi` | https://gitlab.gnome.org/GNOME/at-spi2-core |
| liblouis | `BRAILLE=1` | https://liblouis.io/ and https://github.com/liblouis/liblouis |

## Candidates, not used yet

Not part of any tool. They are here so that a decision to use one starts from the right
documentation and a comparison ([decisions.md](decisions.md), principle 4).

| Library | What it would be for | Installed here | Documentation |
| --- | --- | --- | --- |
| Clay | flexbox-style layout for a graphical tool; a single header | clay 0.14 (`/usr/include/clay.h`) | https://github.com/nicbarker/clay and https://www.nicbarker.com/clay |
| SDL3 | windowing, input and audio in one library, which games also load | sdl3 3.4.18 | https://wiki.libsdl.org/SDL3/FrontPage and https://github.com/libsdl-org/SDL |

## Programs that are run

The tools start these as commands (principle 3) rather than linking their libraries. A missing
one is a missing feature, said once.

| Program | Used by | For | Documentation |
| --- | --- | --- | --- |
| `rsvg-convert` (librsvg 2.63.4) | pacreact | drawing SVG icons | https://gnome.pages.gitlab.gnome.org/librsvg/Rsvg-2.0/ |
| `pw-play`, `paplay` | pacreact | playing sounds | https://www.pipewire.org/ |
| `wpctl` | pacpeek | volumes | https://www.pipewire.org/ |
| `makoctl`, `dunstctl` | pacpeek | counting notifications | https://gitlab.freedesktop.org/emersion/mako |

## Specifications

- Desktop notifications: https://specifications.freedesktop.org/notification-spec/latest/
- XDG base directories (where settings, state and data go): https://specifications.freedesktop.org/basedir-spec/latest/
- greetd's protocol, for pacgreet: https://man.archlinux.org/man/greetd-ipc.7
- Compositors the tools were tested on: https://hyprland.org/, https://swaywm.org/, and pacland
