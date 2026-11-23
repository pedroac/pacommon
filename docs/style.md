# Style of the pac tools

One look for every tool, so that a person who knows one knows them all. This page says what the look
is, where it lives in the code, and which tools still differ (and why). The reasons behind it are
principles [32 to 36](../PRINCIPLES.md); the numbers below are the ones in the code, which is the
authority: if they disagree, the code wins and this page is wrong.

## Where the look lives

| What | Where | Used through |
| --- | --- | --- |
| Colors, normal and high contrast | `src/palette.[ch]` (`PAC_PALETTE_NORMAL`, `PAC_PALETTE_HIGH`) | `PacPalette` |
| Fonts to prefer | `src/fontfind.[ch]` (`PAC_FONTS_REGULAR`, `_BOLD`, `_LIGHT`) | `pac_font_load(path, names, index)` |
| Text sizes, column width, zoom | `src/tunables.h` (`UI_TEXT_MM`, `UI_ZOOM`, `UI_MAX_W`, `UI_MAX_W_WIDE`, `UI_TWO_COLUMNS_MIN`) | `gui_kit` |
| Buttons, focus ring, cards, gauges, scroll bars | `src/gui_kit.[ch]` | `kit_*` |
| The window (layer-shell surface, pointer, keyboard) | `src/gui_win.[ch]` | `win_*` |
| Icons drawn by code, and pictures | `src/gui_icons.[ch]`, `src/gui_img.[ch]` | `gui_*` |
| Icons from the icon theme | `src/icon_theme.[ch]` | `icon_get` |
| Settings that change the look | `pac.conf`: `contrast`, `text_scale`, `text_size_min/max`, `font`, `icons`, `reduce_motion`, `theme` | `docs/configuration.md` |

A tool does not keep a color, a font list or a size of its own when one of these says it. A new
look is made in pacommon, once, and every tool gets it.

## Color

Colors are sRGB, 0 to 1 per channel. Dark background, calm and dim, never pure white text in the
normal palette (principle 33).

| Role | Normal | High contrast | Means |
| --- | --- | --- | --- |
| `background` | 0.075, 0.075, 0.090 | 0, 0, 0 | The surface |
| `text` | 0.95, 0.95, 0.95 | 1, 1, 1 | What is read |
| `label` / `label_focus` | 0.49, 0.50, 0.54 / 0.52, 0.53, 0.57 | 0.68 / 0.85 gray | The name of a field, and the one that has the focus |
| `dim`, `faint` | 0.55, 0.56, 0.60 / 0.50, 0.51, 0.55 | 0.85 / 0.75 gray | Secondary text |
| `rule` | 0.38, 0.39, 0.43 | 0.62 gray | Lines and borders (graphics) |
| `warn` | 0.85, 0.74, 0.42 | 1.0, 0.88, 0.30 | Attention |
| `error` | 0.87, 0.47, 0.44 | 1.0, 0.45, 0.40 | Failure |
| `busy_alpha` | 0.5 | 1.0 | What is left of everything while the tool works |

Rules, checked by `make test` (`pac_palette_contrast_ok`, `pac_palette_meanings_apart`):

- Text is at least 4.5:1 against the background, `rule` at least 3:1 (WCAG 2).
- `warn` and `error` stay apart as seen with protanopia, deuteranopia and tritanopia, and are
  never the only sign: a shape, a word or a symbol goes with them.
- `contrast = high` wins over everything: black and white, nothing dimmed, no transparency.
- Colors mean the same in every tool: yellow is attention, red is failure, blue is information (the
  accent `0.55, 0.74, 1.00`, `0.45, 0.75, 1.00` in high contrast).

## Text

- **Font.** Atkinson Hyperlegible (`ttf-atkinson-hyperlegible`), else Noto Sans, DejaVu Sans,
  Liberation Sans. `font =` in `pac.conf` overrides it. Use `PAC_FONTS_*`; never write the list again.
- **Size.** `UI_TEXT_MM` (4.2 mm) at the screen's real density, times `text_scale` (0.5 to 2.5), kept
  between `text_size_min` and `text_size_max`, and never larger than the screen allows. Sizes are
  physical, not pixels, so a tool is as readable on a 4K laptop as on a desk monitor.
- **Shaping.** All drawn text goes through `textlayout`/`textdraw` (HarfBuzz and FriBidi, a font for each script).
  Never `cairo_show_text` for text a person reads.
- **Weight.** Regular for text, bold for a title or a focus, light only for a large figure such as a clock.

## Layout

- The content is a column in the middle, at most `UI_MAX_W` (860 design px) wide, `UI_MAX_W_WIDE`
  (1480) in two columns when the window is at least `UI_TWO_COLUMNS_MIN` (1240) wide. A label is never far from its value.
- Spacing, rounding and the focus ring come from `gui_kit`; a tool does not draw its own button.
- The focus is always visible (ring), reachable by keyboard, and never only a color change (principle 35).

## Motion and sound

- `reduce_motion = yes` removes every fade, slide and anything that moves by itself, and wins over a
  tool's own setting (pacreact's `fade_ms`).
- A tool with no motion ignores the setting; it never needs to say so.

## Icons

- `icons = auto` is the tool's own choice, `theme` the icon theme's, `0` none. A tool that has no
  icon theme treats `theme` as `auto` quietly.
- From the icon theme: `icon_theme.c` (PNG by Cairo, SVG by `rsvg-convert`, right-to-left variants).
- Drawn by code: `gui_icons.c`, in the text color, so they follow contrast and size.

## Where tools still differ

Written down on 2026-11-23 by comparing the code. Each line is a choice to make (the author's),
not something to fix silently, because a color change is seen by every person who uses the tool.

| Tool | Difference | Why it may be intentional |
| --- | --- | --- |
| pacauth, paconf, pacmenu, pacreact, pacterm | Background `0.045, 0.045, 0.06`, a darker one than the palette's `0.075, 0.075, 0.090`; text 1.0 and soft 0.78, 0.79, 0.83 | A window over the desktop (overlay, translucent) rather than a full screen. Needs a decision: make it `PAC_PALETTE_*.overlay`, or use the palette's |
| pacbar | Text 0.90, 0.90, 0.93; an alert is `0.93, 0.27, 0.27` (the palette's `error` is `0.87, 0.47, 0.44`); does not use `PacPalette` | A bar is small and sits on any wallpaper |
| pacpeek | Own clock, date and level colors (`render.c`) that match the palette's `text`, `label`, `warn`, `error` by value, not by name | Not used `PacPalette` yet |
| pacreact | Own `Palette` in `paint.c` (soft, faint, dim, accents) with the palette's `warn` and `error` values | Same |
| pacland | One accent from `pacland.conf`, every other color derived from it | A compositor's decoration is the person's to color |
| pacterm | A 16-color terminal scheme (normal and high contrast) | A terminal's colors are data |
| paclock, pacgreet | Use the palette but add their own surface colors | |
| `theme` | Documented and accepted as a shared key (`config_file.c`), but no tool reads a color scheme from `themes/` yet | Open: implement in `palette.c`, or remove from the docs |

## Making a tool look right

1. Colors from `PacPalette`, chosen by `contrast`. 2. Fonts from `PAC_FONTS_*` and `textdraw`. 3.
Sizes from `UI_TEXT_MM` and the shared settings. 4. A window from `gui_win`/`gui_kit`. 5. Honor
`reduce_motion`, `text_scale`, `text_size_min/max`, `font`, `language`, `icons`, `speech`. 6. Run the
accessibility checklist in [accessibility.md](accessibility.md).
