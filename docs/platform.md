# Platform support

pacommon targets **Linux, glibc or musl**, and is part of tools that are Wayland only (a compositor may run X11 programs through Xwayland, an option of its build). Linux
comes first: nothing here has `#ifdef`s for other systems.

## What has been tested

The project was started by Pedro Amaral Couto on **Arch Linux**, and the tools using this code
were run on **Hyprland**, **Sway** and **pacland** (the author's own compositor, built on wlroots). Everything else is *untested* until a row below says
otherwise.

| System | libc | `make test` | `make sanitize` | Tested by / when |
| --- | --- | --- | --- | --- |
| Arch Linux (gcc, clang) | glibc | pass | pass | author, 2026-11-15 |
| Debian stable | glibc | not yet | not yet | |
| Ubuntu LTS | glibc | not yet | not yet | |
| Fedora | glibc | not yet | not yet | |
| Alpine | musl | not yet | n/a (no ASan on musl) | |
| openSUSE Tumbleweed | glibc | not yet | n/a | |

`tests/distros.sh` fills in the table: it needs podman or docker and runs each distribution in a
container (`tests/distros.sh docker.io/library/alpine:latest` for one). Update the table by hand
from its output, with the date. A distribution that fails is a bug to fix here, or a limit to
write in the next section.

Speech (`SPEECH=speechd`) has been tested by the author with speech-dispatcher 0.12.1, on Arch Linux, with espeak-ng installed and Piper set as the default module (the engine is the person's choice in `speechd.conf`: both are on that machine, and which one spoke in each test was not recorded). To be heard, speech needs more than the library: a **speech-dispatcher** server and a **speech engine** (a speech-dispatcher module) with a voice, such as **espeak-ng** (small, offline, many languages; an optional dependency of speech-dispatcher on Arch, where it is the usual choice), or Piper or Festival, and a sound server (PulseAudio or PipeWire). Without an engine the server has nothing to speak with.

The build needs `libspeechd` (speech-dispatcher's client library); a distribution that packages it separately (Debian's `libspeechd-dev`) has to be told. A person who needs speech must install an engine themselves unless the distribution's packaging of the tools pulls one in: **no pac tool installs or checks one**, so a system with the server and no engine is silent, and nothing says so. Other
speech-dispatcher versions and distributions are *untested*; `SPEECH=atspi` does not exist yet.

## What the code assumes

- **`explicit_bzero`** (`field.c`) needs glibc 2.25 or musl 1.1.20. It is how a password is wiped,
  so there is no fallback to `memset`, which a compiler may remove.
- **`nftw` with `FTW_F` and symbolic links followed** (`fontfind.c`). `font_find` keeps its state
  in statics and is not thread-safe.
- **Font locations** come from the XDG variables (`$XDG_DATA_HOME`, `$XDG_DATA_DIRS`) and `~/.fonts`,
  with `/usr/local/share:/usr/share` as the default. A distribution that installs fonts somewhere
  else (NixOS, Guix) is found only through `$XDG_DATA_DIRS`.
- **`SYSCONFDIR`** defaults to `/etc`. A distribution that keeps system settings elsewhere
  builds with `-DSYSCONFDIR='"..."'`.
- **evdev key codes** in `field.h` (`key`) are Linux's. The caller supplies them.
- **Hardening flags** in the `Makefile` (`-D_FORTIFY_SOURCE=3`, `-fstack-clash-protection`, `-z
  now`, ...) are written for current GCC and Clang. A toolchain that does not know one fails the
  build; that is reported on the distribution's row, not silenced with a lower level. CET
  (`-fcf-protection=full`) is probed and dropped where the compiler lacks it.
- **Build:** GNU make and a C compiler. No `pkg-config`, no Wayland headers.

## Other compositors and window systems

pacommon has no compositor code. What a tool needs from the compositor is in that tool's
`docs/platform.md`. Hyprland, Sway and pacland are what was tried; other wlroots-based compositors are
expected to behave the same, and GNOME's Mutter lacks `wlr-layer-shell`, so tools that depend on
it cannot run there.
