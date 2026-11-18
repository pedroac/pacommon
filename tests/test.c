/* Unit tests for the shared code. None of it needs a display. */
#define _GNU_SOURCE
#include "atomic_file.h"
#include "config_file.h"
#include "field.h"
#include "fontfind.h"
#include "language.h"
#include "layout.h"
#include "layout_detect.h"
#include "layout_text.h"
#include "length.h"
#include "palette.h"
#include "setting.h"
#include "textinput.h"
#include "speech.h"
#include "utf8.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks, failures;
static FILE *report;   /* the real stderr: the library's own messages are silenced */

#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { failures++; fprintf(report, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

#define CHECK_STR(actual, expected) do { \
    checks++; \
    if (strcmp((actual), (expected)) != 0) { \
        failures++; \
        fprintf(report, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (actual), (expected)); \
    } \
} while (0)

static char tmp[256];

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(2); }
    fputs(text, f);
    fclose(f);
}

static void make_dir(const char *path) {
    if (mkdir(path, 0700) != 0) { perror(path); exit(2); }
}

/* ---- config_file ---- */

struct seen {
    char keys[8][32], values[8][32];
    int count;
};

static bool collect(const char *key, const char *value, void *data) {
    struct seen *s = data;
    if (strcmp(key, "bad") == 0) return false;
    if (s->count < 8) {
        snprintf(s->keys[s->count], 32, "%s", key);
        snprintf(s->values[s->count], 32, "%s", value);
        s->count++;
    }
    return true;
}

static void test_config_file(void) {
    char path[300];
    snprintf(path, sizeof path, "%s/a.conf", tmp);

    write_file(path,
        "# a comment\n"
        "\n"
        "font = Sans Bold   # trailing comment\n"
        "  text_scale=1.5\n"
        "empty =\n"
        "url = a=b\n");
    struct seen s = { .count = 0 };
    CHECK(pac_config_load_file("t", path, collect, &s) == 0);
    CHECK(s.count == 4);
    CHECK_STR(s.keys[0], "font");
    CHECK_STR(s.values[0], "Sans Bold");
    CHECK_STR(s.keys[1], "text_scale");
    CHECK_STR(s.values[1], "1.5");
    CHECK_STR(s.values[2], "");
    CHECK_STR(s.values[3], "a=b");   /* only the first '=' splits */

    /* Bad lines are skipped, and the lines after them still count. */
    write_file(path, "no equals sign\nbad = 1\ngood = 2\n");
    s.count = 0;
    CHECK(pac_config_load_file("t", path, collect, &s) == 2);
    CHECK(s.count == 1 && strcmp(s.keys[0], "good") == 0);

    /* A line too long for the buffer is skipped whole: its tail is not read as a setting. */
    FILE *f = fopen(path, "w");
    fputs("long = ", f);
    for (int i = 0; i < 3000; i++) fputc('x', f);
    fputs("\ntail = 1\nafter = 2\n", f);
    fclose(f);
    s.count = 0;
    CHECK(pac_config_load_file("t", path, collect, &s) == 1);
    CHECK(s.count == 2 && strcmp(s.keys[0], "tail") == 0);

    /* A last line without a newline is read. */
    write_file(path, "last = 1");
    s.count = 0;
    CHECK(pac_config_load_file("t", path, collect, &s) == 0 && s.count == 1);

    CHECK(pac_config_load_file("t", "/nonexistent/x.conf", collect, &s) == -1);
}

static void test_config_path(void) {
    char path[300];
    setenv("HOME", "/home/u", 1);
    unsetenv("XDG_CONFIG_HOME");
    CHECK(pac_config_path("/etc", "tool", 0, path, sizeof path));
    CHECK_STR(path, "/etc/pac/tool.conf");
    CHECK(pac_config_path(NULL, "tool", 0, path, sizeof path));
    CHECK_STR(path, SYSCONFDIR "/pac/tool.conf");
    CHECK(pac_config_path("/etc", "tool", 1, path, sizeof path));
    CHECK_STR(path, "/home/u/.config/pac/tool.conf");
    CHECK(pac_config_path("/etc", "tool", 2, path, sizeof path));
    CHECK_STR(path, "tool.conf");
    CHECK(!pac_config_path("/etc", "tool", 3, path, sizeof path));
    CHECK(!pac_config_path("/etc", "tool", -1, path, sizeof path));

    setenv("XDG_CONFIG_HOME", "/xdg", 1);
    CHECK(pac_config_path("/etc", "tool", 1, path, sizeof path));
    CHECK_STR(path, "/xdg/pac/tool.conf");

    /* An empty XDG_CONFIG_HOME counts as unset; with no HOME either there is no location. */
    setenv("XDG_CONFIG_HOME", "", 1);
    CHECK(pac_config_path("/etc", "tool", 1, path, sizeof path));
    CHECK_STR(path, "/home/u/.config/pac/tool.conf");
    unsetenv("HOME");
    CHECK(!pac_config_path("/etc", "tool", 1, path, sizeof path));

    /* A path that does not fit is refused, not cut. */
    CHECK(!pac_config_path("/etc", "tool", 0, path, 8));
}

static void test_config_load(void) {
    /* The three locations override each other, lowest priority first. */
    char dir[300], cwd[300];
    snprintf(dir, sizeof dir, "%s/home", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home/pac", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home/pac/tool.conf", tmp);
    write_file(dir, "a = user\nb = user\n");
    snprintf(dir, sizeof dir, "%s/work", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/work/tool.conf", tmp);
    write_file(dir, "b = here\n");

    snprintf(dir, sizeof dir, "%s/home", tmp);
    setenv("XDG_CONFIG_HOME", dir, 1);
    snprintf(dir, sizeof dir, "%s/work", tmp);
    if (!getcwd(cwd, sizeof cwd) || chdir(dir) != 0) { perror("chdir"); exit(2); }

    struct seen s = { .count = 0 };
    pac_config_load("tool", collect, &s);
    CHECK(s.count >= 3);
    const char *a = NULL, *b = NULL;
    for (int i = 0; i < s.count; i++) {   /* the last value wins, as a tool's handler would see it */
        if (strcmp(s.keys[i], "a") == 0) a = s.values[i];
        if (strcmp(s.keys[i], "b") == 0) b = s.values[i];
    }
    CHECK(a && strcmp(a, "user") == 0);
    CHECK(b && strcmp(b, "here") == 0);
    if (chdir(cwd) != 0) exit(2);
}

/* ---- setting ---- */

static void test_setting(void) {
    CHECK(pac_setting("font") == NULL);
    CHECK(pac_setting_set("font", "Sans"));
    CHECK_STR(pac_setting("font"), "Sans");
    CHECK(pac_setting_set("font", "Serif"));   /* replaces */
    CHECK_STR(pac_setting("font"), "Serif");
    CHECK(pac_setting_set("language", "pt"));
    CHECK_STR(pac_setting("language"), "pt");
    CHECK(pac_setting_set("font", NULL));      /* unsets only that one */
    CHECK(pac_setting("font") == NULL);
    CHECK_STR(pac_setting("language"), "pt");
    CHECK(pac_setting_set("missing", NULL));   /* unsetting what is not there is fine */
    CHECK(pac_setting_store("contrast", "high", NULL));
    CHECK_STR(pac_setting("contrast"), "high");

    /* As a handler of a file. */
    char path[300];
    snprintf(path, sizeof path, "%s/s.conf", tmp);
    write_file(path, "theme = dark\n");
    CHECK(pac_config_load_file("t", path, pac_setting_store, NULL) == 0);
    CHECK_STR(pac_setting("theme"), "dark");

    pac_setting_set("language", NULL);
    pac_setting_set("contrast", NULL);
    pac_setting_set("theme", NULL);
}

/* ---- field ---- */

static bool translate(uint32_t key, uint32_t mods, char *out, size_t size, void *data) {
    (void)mods;
    const char *layout = data;   /* "a" types the same letter; "b" types the next one, or "é" for 30 */
    if (strcmp(layout, "a") == 0) { out[0] = (char)('a' + key - 30); out[1] = '\0'; return size > 1; }
    if (key == 30) { snprintf(out, size, "\xC3\xA9"); return true; }
    if (key == 99) return false;
    if (key == 98) { snprintf(out, size, "\x01"); return true; }   /* a control character */
    out[0] = (char)('b' + key - 30);
    out[1] = '\0';
    return true;
}

static void test_field(void) {
    Field f;
    field_clear(&f);
    CHECK(field_chars(&f) == 0);
    CHECK(field_append(&f, "ab"));
    CHECK(field_append(&f, "\xC3\xA9"));          /* two bytes, one character */
    CHECK(field_append(&f, "\xE2\x82\xAC"));      /* three bytes */
    CHECK(field_chars(&f) == 4);
    CHECK_STR(f.text, "ab\xC3\xA9\xE2\x82\xAC");
    field_backspace(&f);
    field_backspace(&f);
    CHECK(field_chars(&f) == 2);
    CHECK_STR(f.text, "ab");
    field_backspace(&f);
    field_backspace(&f);
    field_backspace(&f);   /* nothing left to delete */
    CHECK(field_chars(&f) == 0 && f.len == 0);

    /* Malformed UTF-8 is kept byte by byte, never read past its end. */
    CHECK(field_append(&f, "\xC3"));
    CHECK(field_append(&f, "\xC0\x80"));          /* overlong */
    CHECK(field_append(&f, "\xED\xA0\x80"));      /* a surrogate */
    CHECK(field_chars(&f) == 6);
    field_clear(&f);

    /* A field that is full refuses more and stays as it was. */
    char big[FIELD_MAX];
    memset(big, 'x', sizeof big - 1);
    big[sizeof big - 2] = '\0';                   /* FIELD_MAX - 2 bytes */
    CHECK(field_append(&f, big));
    CHECK(!field_append(&f, "yy"));
    CHECK(f.len == FIELD_MAX - 2);
    CHECK(field_append(&f, "y"));
    CHECK(!field_append(&f, "y"));                /* the terminator needs room too */
    field_clear(&f);

    /* Backspace wipes what it removes: a field may have held a password. */
    field_append(&f, "secret");
    field_backspace(&f);
    CHECK(f.text[5] == '\0' && f.len == 5 && f.typed[5].len == 0);
    field_clear(&f);
    CHECK(f.text[0] == '\0' && f.len == 0 && f.count == 0);
}

static FieldKey press_key(Field *f, uint32_t keysym, const char *text, unsigned held, size_t max) {
    return field_edit_key(f, &(FieldKeyEvent){ keysym, text, 30, 4, held }, max);
}

static void test_field_edit_key(void) {
    enum { BACKSPACE = 0xff08, ESCAPE = 0xff1b };
    Field f;
    field_clear(&f);
    CHECK(press_key(&f, 'a', "a", 0, 0) == FIELD_KEY_EDITED && f.len == 1);
    CHECK(f.typed[0].key == 30 && f.typed[0].mods == 4);               /* kept, so it can be retyped */
    CHECK(press_key(&f, 0, "\xC3\xA9", 0, 0) == FIELD_KEY_EDITED && field_chars(&f) == 2);
    CHECK(press_key(&f, 'A', "A", 0, 0) == FIELD_KEY_EDITED);          /* shift is not a modifier here */
    CHECK_STR(f.text, "a\xC3\xA9" "A");
    CHECK(press_key(&f, BACKSPACE, "\b", 0, 0) == FIELD_KEY_EDITED && field_chars(&f) == 2);
    CHECK(press_key(&f, BACKSPACE, "\b", FIELD_ALT, 0) == FIELD_KEY_EDITED && field_chars(&f) == 1);   /* one character */
    CHECK(press_key(&f, 'b', "b", 0, 0) == FIELD_KEY_EDITED);
    CHECK(press_key(&f, BACKSPACE, "\b", FIELD_CTRL, 0) == FIELD_KEY_EDITED && f.len == 0);   /* all of it */

    /* Nothing to change is not an edit, so a caller does not redraw or reset anything for it. */
    CHECK(press_key(&f, BACKSPACE, "\b", 0, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, BACKSPACE, "\b", FIELD_CTRL, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 'u', "u", FIELD_CTRL, 0) == FIELD_KEY_IGNORED);

    CHECK(press_key(&f, 'x', "xyz", 0, 0) == FIELD_KEY_EDITED);
    CHECK(press_key(&f, 'U', "\x15", FIELD_CTRL, 0) == FIELD_KEY_EDITED && f.len == 0);   /* Ctrl+U, either case */
    CHECK(press_key(&f, 'x', "xyz", 0, 0) == FIELD_KEY_EDITED);
    CHECK(press_key(&f, 'u', "\x15", FIELD_CTRL, 0) == FIELD_KEY_EDITED && f.len == 0);

    /* What is not text: a shortcut, a control character, DEL, nothing, a key with no text. */
    CHECK(press_key(&f, 'a', "a", FIELD_CTRL, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 'a', "a", FIELD_ALT, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 'a', "a", FIELD_LOGO, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 'u', "u", FIELD_ALT, 0) == FIELD_KEY_IGNORED);   /* only Ctrl+U clears; Alt+U is a shortcut */
    CHECK(press_key(&f, 'c', "\x03", 0, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, ESCAPE, "\x1b", 0, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 0, "\x7f", 0, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 0, "", 0, 0) == FIELD_KEY_IGNORED);
    CHECK(press_key(&f, 0, NULL, 0, 0) == FIELD_KEY_IGNORED);
    CHECK(f.len == 0);                                                  /* none of them changed it */
    CHECK(press_key(&f, 'u', "u", 0, 0) == FIELD_KEY_EDITED);           /* a plain u is a letter */
    CHECK_STR(f.text, "u");
    field_clear(&f);

    /* A limit of the tool's own: up to max bytes, and a character that does not fit is refused whole. */
    CHECK(press_key(&f, 'a', "ab", 0, 3) == FIELD_KEY_EDITED);
    CHECK(press_key(&f, 'c', "\xC3\xA9", 0, 3) == FIELD_KEY_FULL && f.len == 2);   /* 2 + 2 > 3 */
    CHECK(press_key(&f, 'c', "c", 0, 3) == FIELD_KEY_EDITED && f.len == 3);
    CHECK(press_key(&f, 'd', "d", 0, 3) == FIELD_KEY_FULL && f.len == 3);
    CHECK(press_key(&f, BACKSPACE, "\b", 0, 3) == FIELD_KEY_EDITED && f.len == 2);   /* a full field still deletes */
    CHECK(press_key(&f, 'd', "d", 0, 1) == FIELD_KEY_FULL && f.len == 2);   /* already longer than the limit */
    field_clear(&f);

    /* The size of a Field is the limit when there is none, or when a bigger one is asked for. */
    for (size_t i = 0; i < FIELD_MAX - 1; i++) CHECK(press_key(&f, 'a', "a", 0, 0) == FIELD_KEY_EDITED);
    CHECK(f.len == FIELD_MAX - 1 && f.text[f.len] == '\0');
    CHECK(press_key(&f, 'a', "a", 0, 0) == FIELD_KEY_FULL);
    CHECK(press_key(&f, 'a', "a", 0, FIELD_MAX * 4) == FIELD_KEY_FULL);
    CHECK(f.len == FIELD_MAX - 1);
    CHECK(press_key(&f, 'a', "\xE2\x82\xAC", 0, 0) == FIELD_KEY_FULL && f.len == FIELD_MAX - 1);
    CHECK(press_key(&f, 'u', "\x15", FIELD_CTRL, 0) == FIELD_KEY_EDITED && f.len == 0);

    /* A field that was written to directly is wiped, not trusted. */
    memset(&f, 0x41, sizeof f);
    CHECK(press_key(&f, 'a', "a", 0, 0) == FIELD_KEY_EDITED && f.len == 1 && f.count == 1);
    CHECK_STR(f.text, "a");
    field_clear(&f);
}

static void test_field_retype(void) {
    Field f;
    field_clear(&f);
    /* keys 31, 32 type "b", "c" in layout a and "c", "d" in layout b */
    field_append_key(&f, "b", 31, 0);
    field_append_key(&f, "c", 32, 0);
    field_append(&f, "!");                        /* from nowhere in particular: stays */
    CHECK_STR(f.text, "bc!");

    CHECK(field_retype(&f, translate, "b"));
    CHECK_STR(f.text, "cd!");
    CHECK(!field_retype(&f, translate, "b"));     /* nothing changed the second time */
    CHECK(field_retype(&f, translate, "a"));
    CHECK_STR(f.text, "bc!");
    CHECK(field_chars(&f) == 3);

    /* A different length: the characters stay in step with the bytes. */
    field_clear(&f);
    field_append_key(&f, "a", 30, 0);
    field_append_key(&f, "x", 31, 0);
    CHECK(field_retype(&f, translate, "b"));
    CHECK_STR(f.text, "\xC3\xA9" "c");   /* key 30 is "é" in layout b, key 31 is "c" */
    CHECK(f.len == 3 && field_chars(&f) == 2);
    field_backspace(&f);
    CHECK_STR(f.text, "\xC3\xA9");
    field_backspace(&f);
    CHECK(f.len == 0);

    /* A key that types nothing, or a control character, keeps the old character. */
    field_clear(&f);
    field_append_key(&f, "q", 99, 0);
    field_append_key(&f, "w", 98, 0);
    CHECK(!field_retype(&f, translate, "b"));
    CHECK_STR(f.text, "qw");

    /* Text of several characters is not one key's, so it is not retyped. */
    field_clear(&f);
    field_append_key(&f, "ab", 30, 0);
    CHECK(!field_retype(&f, translate, "b"));
    CHECK_STR(f.text, "ab");
}

/* ---- fontfind ---- */

static void test_fontfind(void) {
    char root[300], sub[300], file[400], found[300];
    snprintf(root, sizeof root, "%s/fonts", tmp);
    make_dir(root);
    snprintf(sub, sizeof sub, "%s/fonts/deep", tmp);
    make_dir(sub);
    snprintf(file, sizeof file, "%s/deep/Second.ttf", root);
    write_file(file, "x");

    const char *dirs[] = { root, NULL };
    const char *want_second[] = { "First.ttf", "Second.ttf", NULL };
    CHECK(font_find(dirs, want_second, found, sizeof found));
    CHECK_STR(found, file);                       /* found in a subfolder */

    /* The earliest name wins wherever it is. */
    char first[400];
    snprintf(first, sizeof first, "%s/First.ttf", root);
    write_file(first, "x");
    CHECK(font_find(dirs, want_second, found, sizeof found));
    CHECK_STR(found, first);

    const char *nothing[] = { "Nope.ttf", NULL };
    CHECK(!font_find(dirs, nothing, found, sizeof found));
    const char *nodirs[] = { "/nonexistent", NULL };
    CHECK(!font_find(nodirs, want_second, found, sizeof found));
    CHECK(!font_find(dirs, want_second, found, 4));   /* does not fit */

    /* A link to a folder is followed, and a link back to its parent does not loop. */
    char link[400];
    snprintf(link, sizeof link, "%s/loop", sub);
    CHECK(symlink(root, link) == 0);
    CHECK(font_find(dirs, nothing, found, sizeof found) == false);

    /* An unreadable file does not count. */
    char hidden[400];
    snprintf(hidden, sizeof hidden, "%s/Hidden.ttf", root);
    write_file(hidden, "x");
    chmod(hidden, 0);
    const char *want_hidden[] = { "Hidden.ttf", NULL };
    CHECK(geteuid() == 0 || !font_find(dirs, want_hidden, found, sizeof found));
}

static void test_font_dirs(void) {
    FontDirs d;
    setenv("HOME", "/home/u", 1);
    setenv("XDG_DATA_HOME", "/data", 1);
    setenv("XDG_DATA_DIRS", "/one:/two::/three", 1);
    font_dirs_init(&d);
    CHECK_STR(d.list[0], "/data/fonts");
    CHECK_STR(d.list[1], "/home/u/.fonts");
    CHECK_STR(d.list[2], "/one/fonts");
    CHECK_STR(d.list[3], "/two/fonts");
    CHECK_STR(d.list[4], "/three/fonts");         /* the empty entry is skipped */
    CHECK(d.list[5] == NULL);

    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_DATA_DIRS");
    font_dirs_init(&d);
    CHECK_STR(d.list[0], "/home/u/.local/share/fonts");
    CHECK_STR(d.list[1], "/home/u/.fonts");
    CHECK_STR(d.list[2], "/usr/local/share/fonts");
    CHECK_STR(d.list[3], "/usr/share/fonts");
    CHECK(d.list[4] == NULL);

    /* Never more than FONT_MAX_DIRS, always NULL-terminated. */
    char many[512] = "";
    for (int i = 0; i < 40; i++) strcat(many, "/d:");
    setenv("XDG_DATA_DIRS", many, 1);
    font_dirs_init(&d);
    int n = 0;
    while (d.list[n]) n++;
    CHECK(n == FONT_MAX_DIRS);
}


/* ---- the settings that every tool shares ---- */

struct shared_seen {
    char keys[16][32], values[16][32];
    int count, refused;
};

static bool shared_collect(const char *key, const char *value, void *data) {
    struct shared_seen *s = data;
    if (strcmp(key, "speech") == 0) { s->refused++; return false; }   /* this tool cannot speak */
    if (s->count < 16) {
        snprintf(s->keys[s->count], 32, "%s", key);
        snprintf(s->values[s->count], 32, "%s", value);
        s->count++;
    }
    return true;
}

static bool collect_shared_example(const char *key, const char *value, void *data) {
    struct shared_seen *s = data;
    if (!pac_config_shared_key(key) || !*value) return false;
    s->count++;
    return true;
}

static const char *last_value(const struct shared_seen *s, const char *key) {
    const char *found = NULL;
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->keys[i], key) == 0) found = s->values[i];
    return found;
}

static void test_config_bool(void) {
    bool v = false;
    const char *yes[] = { "yes", "true", "on", "1", "Yes", "TRUE", "On" };
    const char *no[] = { "no", "false", "off", "0", "No", "FALSE", "Off" };
    for (size_t i = 0; i < sizeof yes / sizeof yes[0]; i++) { v = false; CHECK(pac_config_bool(yes[i], &v) && v); }
    for (size_t i = 0; i < sizeof no / sizeof no[0]; i++) { v = true; CHECK(pac_config_bool(no[i], &v) && !v); }
    v = true;
    CHECK(!pac_config_bool("maybe", &v) && v);                       /* refused: untouched */
    CHECK(!pac_config_bool("", &v) && !pac_config_bool("yess", &v) && !pac_config_bool("2", &v) && v);
    CHECK(pac_config_bool("yes", NULL) && !pac_config_bool("maybe", NULL));   /* only checking */
}

static void test_config_shared(void) {
    CHECK(pac_config_shared_key("text_scale") && pac_config_shared_key("braille_table") && pac_config_shared_key("reduce_motion"));
    CHECK(!pac_config_shared_key("window_width") && !pac_config_shared_key("") && !pac_config_shared_key(NULL));

    char dir[300], cwd[300];
    snprintf(dir, sizeof dir, "%s/home2", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home2/pac", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home2/pac/pac.conf", tmp);
    write_file(dir, "font = Shared Sans\ntext_scale = 1.5\nspeech = yes\nlanguage = pt_BR\n"
                    "window_width = 100\ncontrast = high\ncontrast = yes\nlow_memory = maybe\n"
                    "text_scale_x = 2\n"
                    "privacy = medium\ntext_size_min = 3mm\ntext_size_max = big\nresources = low\nprivacy = extreme\n"
                    "text_size_max = 0.5in\nresources = minimal\nprivacy = 2\n");
    snprintf(dir, sizeof dir, "%s/home2/pac/tool.conf", tmp);
    write_file(dir, "font = Own Serif\nwindow_width = 200\n");
    snprintf(dir, sizeof dir, "%s/work2", tmp);
    make_dir(dir);

    snprintf(dir, sizeof dir, "%s/home2", tmp);
    setenv("XDG_CONFIG_HOME", dir, 1);
    snprintf(dir, sizeof dir, "%s/work2", tmp);
    if (!getcwd(cwd, sizeof cwd) || chdir(dir) != 0) { perror("chdir"); exit(2); }

    struct shared_seen s = { .count = 0 };
    pac_config_load("tool", shared_collect, &s);
    CHECK_STR(last_value(&s, "font"), "Own Serif");        /* the tool's own file beats pac.conf */
    CHECK_STR(last_value(&s, "text_scale"), "1.5");        /* what the tool does not set is shared */
    CHECK_STR(last_value(&s, "language"), "pt_BR");
    CHECK_STR(last_value(&s, "contrast"), "high");         /* normal/high; "yes" is refused */
    CHECK_STR(last_value(&s, "window_width"), "200");      /* from tool.conf only: not shared */
    CHECK(s.count > 0 && strcmp(s.keys[0], "font") == 0 && strcmp(s.values[0], "Shared Sans") == 0);   /* first */
    CHECK_STR(last_value(&s, "text_size_min"), "3mm");     /* a length with a unit */
    CHECK_STR(last_value(&s, "text_size_max"), "0.5in");   /* "big" is refused, the later line stands */
    CHECK_STR(last_value(&s, "resources"), "minimal");     /* the three levels... */
    CHECK_STR(last_value(&s, "privacy"), "medium");        /* ...four for privacy; extreme and 2 are refused */
    CHECK(s.refused == 1);                                 /* speech was offered, and refused silently */
    for (int i = 0; i < s.count; i++) {                    /* not shared, or a bad value: never delivered */
        CHECK(strcmp(s.keys[i], "text_scale_x") != 0);
        CHECK(!(strcmp(s.keys[i], "window_width") == 0 && strcmp(s.values[i], "100") == 0));
    }

    /* A tool that reads its own files only still gets the shared ones if it asks. */
    struct shared_seen only = { .count = 0 };
    pac_config_load_shared("tool", shared_collect, &only);
    CHECK_STR(last_value(&only, "font"), "Shared Sans");
    CHECK(last_value(&only, "window_width") == NULL);
    pac_config_load_shared("tool", NULL, NULL);            /* no handler: nothing to do, no crash */

    /* The current folder's pac.conf is read, unless the tool leaves that location out. */
    write_file("pac.conf", "font = Cwd Sans\nwindow_width = 5\n");
    struct shared_seen with_cwd = { .count = 0 }, without = { .count = 0 }, one = { .count = 0 };
    pac_config_load_shared("tool", shared_collect, &with_cwd);
    CHECK_STR(last_value(&with_cwd, "font"), "Cwd Sans");
    pac_config_load_shared_locations("tool", PAC_CONFIG_LOCATIONS_NO_CWD, shared_collect, &without);
    CHECK_STR(last_value(&without, "font"), "Shared Sans");
    CHECK(last_value(&without, "window_width") == NULL);
    pac_config_load_shared_file("tool", "pac.conf", shared_collect, &one);   /* one file, shared keys only */
    CHECK_STR(last_value(&one, "font"), "Cwd Sans");
    CHECK(last_value(&one, "window_width") == NULL);
    unlink("pac.conf");

    if (chdir(cwd) != 0) exit(2);
}

/* pac.conf, the example, says what the code accepts: every shared key is in it, and every line of
 * it, uncommented, is accepted (principle 23). */
static void test_pac_conf_example(void) {
    FILE *f = fopen("pac.conf", "r");
    if (!f) { fprintf(report, "FAIL pac.conf not found (run from the pacommon folder)\n"); failures++; return; }
    static const char *const keys[] = { "contrast", "text_scale", "text_size_min", "text_size_max", "font",
                                        "language", "icons", "theme", "speech", "resources", "privacy",
                                        "braille_table", "reduce_motion" };
    enum { KEYS = sizeof keys / sizeof keys[0] };
    int seen[KEYS] = { 0 }, lines = 0;
    char line[512], out[300];
    char dir[300], cwd[300];
    snprintf(dir, sizeof dir, "%s/home3", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home3/pac", tmp);
    make_dir(dir);
    snprintf(out, sizeof out, "%s/home3/pac/pac.conf", tmp);
    FILE *o = fopen(out, "w");
    while (fgets(line, sizeof line, f)) {
        if (line[0] != '#' || !isalpha((unsigned char)line[1])) continue;   /* explanations stay comments */
        char key[40];
        if (sscanf(line + 1, "%39[a-z_] =", key) != 1) continue;
        fputs(line + 1, o);
        lines++;
        for (int i = 0; i < KEYS; i++) if (strcmp(key, keys[i]) == 0) seen[i]++;
    }
    fclose(f);
    fclose(o);
    CHECK(lines == KEYS);
    for (int i = 0; i < KEYS; i++) CHECK(seen[i] == 1 && pac_config_shared_key(keys[i]));

    /* Read as the shared file of a tool, whose values are checked first: all of them get through. */
    snprintf(dir, sizeof dir, "%s/home3", tmp);
    setenv("XDG_CONFIG_HOME", dir, 1);
    snprintf(dir, sizeof dir, "%s/home3/pac", tmp);   /* a folder with no pac.conf in it to stand in */
    if (!getcwd(cwd, sizeof cwd) || chdir(dir) != 0) { perror("chdir"); exit(2); }
    struct shared_seen s = { .count = 0 };
    pac_config_load_shared("t", collect_shared_example, &s);
    CHECK(s.count >= KEYS);                          /* a system pac.conf, if there is one, may add */
    if (chdir(cwd) != 0) exit(2);
}

/* ---- hostile and broken input ---- */

static void test_config_guards(void) {
    char path[300];
    snprintf(path, sizeof path, "%s/g.conf", tmp);
    struct seen s = { .count = 0 };

    /* A NUL ends a line for every string function: the line is skipped whole and the next one is
     * still read (it used to be taken for the rest of a long line, and lost). */
    FILE *f = fopen(path, "wb");
    fwrite("a = 1\0junk\nb = 2\nc = 3\n", 1, 23, f);
    fclose(f);
    CHECK(pac_config_load_file("t", path, collect, &s) == 1);
    CHECK(s.count == 2 && strcmp(s.keys[0], "b") == 0 && strcmp(s.keys[1], "c") == 0);

    /* Only a regular file: a pipe would block the tool, a device never ends. */
    char fifo[300];
    snprintf(fifo, sizeof fifo, "%s/pipe.conf", tmp);
    CHECK(mkfifo(fifo, 0600) == 0);
    CHECK(pac_config_load_file("t", fifo, collect, &s) == -1);   /* returns, and does not hang */
    CHECK(pac_config_load_file("t", "/dev/zero", collect, &s) == -1);
    CHECK(pac_config_load_file("t", tmp, collect, &s) == -1);    /* a folder */

    /* A file over the limit is not read at all. */
    f = fopen(path, "w");
    for (int i = 0; i < PAC_CONFIG_MAX_SIZE / 8 + 10; i++) fputs("k = 12345\n", f);
    fclose(f);
    s.count = 0;
    CHECK(pac_config_load_file("t", path, collect, &s) == -1 && s.count == 0);

    /* A file that is exactly at the limit is read. */
    f = fopen(path, "w");
    for (int i = 0; i < PAC_CONFIG_MAX_SIZE / 8; i++) fputs("k = 123\n", f);
    fclose(f);
    CHECK(pac_config_load_file("t", path, collect, &s) == 0 && s.count == 8);   /* the collector keeps 8 */

    /* A handler or names that are NULL are refused, not followed. */
    CHECK(pac_config_load_file("t", path, NULL, &s) == -1);
    CHECK(pac_config_load_file("t", NULL, collect, &s) == -1);
    CHECK(pac_config_load_file(NULL, path, collect, &s) == -1);

    /* The name of the tool is a file name, never a path. */
    char p[300];
    CHECK(!pac_config_path("/etc", "../x", 0, p, sizeof p));
    CHECK(!pac_config_path("/etc", "a/b", 2, p, sizeof p));
    CHECK(!pac_config_path("/etc", "", 0, p, sizeof p));
    CHECK(!pac_config_path("/etc", NULL, 0, p, sizeof p));
    CHECK(!pac_config_path("/etc", "tool", 0, NULL, 10));
    CHECK(!pac_config_path("/etc", "tool", 0, p, 0));
}

static void test_setting_guards(void) {
    pac_setting_clear();
    CHECK(pac_setting(NULL) == NULL);
    CHECK(!pac_setting_set(NULL, "x"));
    CHECK(!pac_setting_set("", "x"));
    char key[PAC_SETTING_KEY_MAX + 2], value[PAC_SETTING_VALUE_MAX + 2];
    memset(key, 'k', sizeof key - 1);
    key[sizeof key - 1] = '\0';
    memset(value, 'v', sizeof value - 1);
    value[sizeof value - 1] = '\0';
    CHECK(!pac_setting_set(key, "x"));             /* a key one byte too long */
    key[PAC_SETTING_KEY_MAX] = '\0';
    CHECK(pac_setting_set(key, "x"));              /* at the limit */
    CHECK(!pac_setting_set("a", value));           /* a value one byte too long */
    value[PAC_SETTING_VALUE_MAX] = '\0';
    CHECK(pac_setting_set("a", "short"));
    value[PAC_SETTING_VALUE_MAX] = 'v';
    CHECK(!pac_setting_set("a", value));           /* a refused value leaves the old one */
    CHECK_STR(pac_setting("a"), "short");
    value[PAC_SETTING_VALUE_MAX] = '\0';
    CHECK(pac_setting_set("a", value));            /* at the limit */
    pac_setting_clear();
    CHECK(pac_setting("a") == NULL);

    /* The block holds only so much: the rest are refused, and what is there stays. */
    char k[32];
    int ok = 0;
    for (int i = 0; i < PAC_SETTING_POOL; i++) {
        snprintf(k, sizeof k, "key%d", i);
        if (!pac_setting_set(k, "value")) break;
        ok++;
    }
    CHECK(ok > 100 && ok < PAC_SETTING_POOL);
    CHECK_STR(pac_setting("key0"), "value");
    snprintf(k, sizeof k, "key%d", ok - 1);
    CHECK_STR(pac_setting(k), "value");
    snprintf(k, sizeof k, "key%d", ok);
    CHECK(pac_setting(k) == NULL);                 /* the one that did not fit is not half there */
    CHECK(pac_setting_set("key0", "newer"));       /* a value of the same size still replaces */
    CHECK_STR(pac_setting("key0"), "newer");
    CHECK(!pac_setting_set("key0", "a value much too long to fit in what is left"));
    CHECK_STR(pac_setting("key0"), "newer");       /* and a refused one leaves the old */
    CHECK(pac_setting_set("key1", NULL));          /* unsetting makes room */
    CHECK(pac_setting_set("another", "v"));
    CHECK_STR(pac_setting("key2"), "value");       /* the others are where they were */
    pac_setting_clear();
    CHECK(pac_setting("key0") == NULL);
    CHECK(pac_setting_set("again", "v") && pac_setting("again") != NULL);

    /* An empty value is a value, and keys that start alike are different keys. */
    pac_setting_clear();
    CHECK(pac_setting_set("font", "") && pac_setting("font") != NULL && *pac_setting("font") == '\0');
    CHECK(pac_setting_set("font_size", "12") && pac_setting_set("fon", "x"));
    CHECK_STR(pac_setting("font_size"), "12");
    CHECK_STR(pac_setting("fon"), "x");
    CHECK(pac_setting("fo") == NULL);
    pac_setting_clear();
}

static bool no_terminator(uint32_t key, uint32_t mods, char *out, size_t size, void *data) {
    (void)key; (void)mods; (void)data;
    memset(out, 'x', size);                        /* fills it all, ends nothing */
    return true;
}

static void test_field_guards(void) {
    Field f;
    field_clear(&f);
    CHECK(!field_append(&f, NULL));
    CHECK(!field_append_key(&f, NULL, 1, 0));

    /* A callback that does not end its text is not read past its buffer. */
    field_append_key(&f, "a", 30, 0);
    CHECK(!field_retype(&f, no_terminator, NULL) || f.len < FIELD_MAX);
    CHECK(!field_retype(&f, NULL, NULL));

    /* A field that was written to directly, or is not initialized, is wiped, not trusted. */
    field_clear(&f);
    field_append(&f, "abc");
    f.count = 99;                                  /* more characters than bytes */
    CHECK(!field_append(&f, "d") || f.len == 1);
    CHECK(f.count <= f.len);

    field_clear(&f);
    field_append(&f, "abc");
    f.len = FIELD_MAX + 5;
    field_backspace(&f);
    CHECK(f.len == 0 && f.count == 0 && f.text[0] == '\0');

    field_clear(&f);
    field_append_key(&f, "a", 30, 0);
    field_append_key(&f, "b", 31, 0);
    f.typed[0].len = 200;                          /* the lengths do not add up to the text */
    CHECK(!field_retype(&f, translate, "b"));
    CHECK(f.len == 0 && f.count == 0);

    field_clear(&f);
    field_append_key(&f, "a", 30, 0);
    f.typed[0].len = 0;
    CHECK(!field_retype(&f, translate, "b"));
    CHECK(f.len == 0);

    memset(&f, 0x55, sizeof f);                    /* uninitialized memory */
    field_backspace(&f);
    CHECK(f.len == 0 && f.count == 0);
}

static void test_fontfind_guards(void) {
    char found[100];
    const char *names[] = { "x.ttf", NULL };
    const char *dirs[] = { tmp, NULL };
    CHECK(!font_find(NULL, names, found, sizeof found));
    CHECK(!font_find(dirs, NULL, found, sizeof found));
    CHECK(!font_find(dirs, names, NULL, sizeof found));
    CHECK(!font_find(dirs, names, found, 0));

    /* A relative entry would search whatever folder the tool was started in. */
    FontDirs d;
    setenv("HOME", "relative/home", 1);
    setenv("XDG_DATA_HOME", "relative/data", 1);
    setenv("XDG_DATA_DIRS", "relative:/abs", 1);
    font_dirs_init(&d);
    CHECK_STR(d.list[0], "/abs/fonts");
    CHECK(d.list[1] == NULL);
}

/* ---- the preedit of a field, and what an input method does to it ---- */

static void test_field_preedit(void) {
    Field f;
    field_clear(&f);
    CHECK_STR(field_preedit(&f), "");
    CHECK(field_append(&f, "ab"));
    CHECK(field_set_preedit(&f, "\xE3\x81\x82\xE3\x81\x84", 3, 6));            /* two kana, the cursor on the second */
    CHECK_STR(field_preedit(&f), "\xE3\x81\x82\xE3\x81\x84");
    CHECK(f.preedit_begin == 3 && f.preedit_end == 6);
    CHECK_STR(f.text, "ab");                                                    /* shown after the text, not part of it */
    CHECK(field_chars(&f) == 2);
    CHECK(field_set_preedit(&f, "x", 1, 1) && f.preedit_begin == 1 && f.preedit_end == 1);   /* replaced, never added */
    CHECK_STR(field_preedit(&f), "x");
    CHECK(field_set_preedit(&f, "", -1, -1) && field_preedit(&f)[0] == '\0');   /* an empty one clears */
    CHECK(field_set_preedit(&f, "abc", -1, -1) && f.preedit_begin == -1 && f.preedit_end == -1);   /* no cursor shown */
    field_clear_preedit(&f);
    CHECK_STR(field_preedit(&f), "");

    /* A cursor that does not fit the text is none: out of it, backwards, or in the middle of a character. */
    CHECK(field_set_preedit(&f, "\xC3\xA9" "a", 5, 5) && f.preedit_begin == -1);
    CHECK(field_set_preedit(&f, "\xC3\xA9" "a", 2, 1) && f.preedit_begin == -1);
    CHECK(field_set_preedit(&f, "\xC3\xA9" "a", 1, 1) && f.preedit_begin == -1);      /* inside the two-byte letter */
    CHECK(field_set_preedit(&f, "\xC3\xA9" "a", -3, 1) && f.preedit_begin == -1);
    CHECK(field_set_preedit(&f, "\xC3\xA9" "a", 3, 3) && f.preedit_begin == 3 && f.preedit_end == 3);   /* the end is fine */

    /* What is not text is refused, and leaves no preedit: bad UTF-8, a control character, a truncated sequence. */
    CHECK(!field_set_preedit(&f, "a\xFF" "b", 0, 0) && field_preedit(&f)[0] == '\0');
    CHECK(!field_set_preedit(&f, "a\nb", 0, 0) && field_preedit(&f)[0] == '\0');
    CHECK(!field_set_preedit(&f, "a\x7f", 0, 0) && field_preedit(&f)[0] == '\0');
    CHECK(!field_set_preedit(&f, "\xE3\x81", 0, 0) && field_preedit(&f)[0] == '\0');
    CHECK(!field_set_preedit(&f, "\xC0\x80", 0, 0));                           /* an overlong NUL */
    CHECK(field_set_preedit(&f, NULL, 0, 0) && field_preedit(&f)[0] == '\0');

    /* Too long: cut at a character, never in the middle of one, and never past the buffer. */
    char big[FIELD_PREEDIT_MAX * 2];
    for (size_t i = 0; i + 3 <= sizeof big - 1; i += 3) memcpy(big + i, "\xE3\x81\x82", 3);
    big[(sizeof big - 1) / 3 * 3] = '\0';
    CHECK(field_set_preedit(&f, big, 0, 0));
    size_t n = strlen(field_preedit(&f));
    CHECK(n > 0 && n < FIELD_PREEDIT_MAX && n % 3 == 0 && f.preedit_len == n);

    /* It is wiped with the field, since a field may have held a password. */
    CHECK(field_set_preedit(&f, "secret", 0, 6));
    field_clear(&f);
    CHECK_STR(field_preedit(&f), "");
    bool zero = true;
    for (size_t i = 0; i < sizeof f.preedit; i++) zero = zero && f.preedit[i] == 0;
    CHECK(zero);

    /* A field written to directly is not trusted: no unterminated preedit is read. */
    memset(f.preedit, 'x', sizeof f.preedit);
    f.preedit_len = sizeof f.preedit - 1;
    CHECK_STR(field_preedit(&f), "");                                           /* not terminated: ignored */
    field_clear(&f);

    /* Deleting from the end: whole characters, and never more than asked. */
    CHECK(field_append(&f, "a\xC3\xA9\xE2\x82\xAC"));                           /* 1, 2 and 3 bytes */
    CHECK(field_delete_before(&f, 2) == 0);                                     /* the euro sign is three bytes: not cut */
    CHECK(field_delete_before(&f, 3) == 3 && f.len == 3 && field_chars(&f) == 2);
    CHECK(field_delete_before(&f, 100) == 3 && f.len == 0);
    CHECK(field_delete_before(&f, 5) == 0);                                     /* nothing left */
}

typedef struct { char log[512]; Field *field; } Sink;

static void sink_preedit(void *data, const char *t, int b, int e) {
    Sink *s = data;
    snprintf(s->log + strlen(s->log), sizeof s->log - strlen(s->log), "P[%s,%d,%d]", t, b, e);
    if (s->field) field_set_preedit(s->field, t, b, e);
}
static void sink_commit(void *data, const char *t) {
    Sink *s = data;
    snprintf(s->log + strlen(s->log), sizeof s->log - strlen(s->log), "C[%s]", t);
    if (s->field) field_append(s->field, t);
}
static void sink_delete(void *data, size_t n) {
    Sink *s = data;
    snprintf(s->log + strlen(s->log), sizeof s->log - strlen(s->log), "D[%zu]", n);
    if (s->field) field_delete_before(s->field, n);
}

static void test_textinput(void) {
    static const PacTextInputSink ops = { sink_preedit, sink_commit, sink_delete };
    PacTextInputPending p;
    Sink s = { "", NULL };
    pac_textinput_pending_reset(&p);

    /* Nothing pending: nothing happens. */
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "");

    /* The order of the protocol: the old preedit goes, the deletion, the commit, and then the new preedit, whatever
     * order the events came in. */
    pac_textinput_pending_preedit(&p, "ni", 2, 2);
    pac_textinput_pending_commit(&p, "\xE4\xBD\xA0");
    pac_textinput_pending_delete(&p, 3, 0);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]D[3]C[\xE4\xBD\xA0]P[ni,2,2]");

    /* It was reset: a second `done` does nothing. */
    s.log[0] = '\0';
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "");

    /* An empty preedit event removes the one that was there; a commit alone also removes it. */
    pac_textinput_pending_preedit(&p, "", -1, -1);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]");
    s.log[0] = '\0';
    pac_textinput_pending_commit(&p, "ok");
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]C[ok]");

    /* Against a field: compose, commit, compose again, and the text only changes at the commit. */
    Field f;
    field_clear(&f);
    s = (Sink){ "", &f };
    pac_textinput_pending_preedit(&p, "ka", 2, 2);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(f.text, "") ; CHECK_STR(field_preedit(&f), "ka");
    pac_textinput_pending_preedit(&p, "kan", 3, 3);                              /* the next state replaces it */
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(field_preedit(&f), "kan");
    pac_textinput_pending_commit(&p, "\xE6\xBC\xA2");
    pac_textinput_pending_preedit(&p, "", -1, -1);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(f.text, "\xE6\xBC\xA2") ; CHECK_STR(field_preedit(&f), "");
    pac_textinput_pending_delete(&p, 3, 99);                                     /* what is after is not asked for */
    pac_textinput_apply(&p, &ops, &s);
    CHECK(f.len == 0);

    /* What comes from outside: refused whole, never half. */
    s = (Sink){ "", NULL };
    pac_textinput_pending_commit(&p, "a\xFF" "b");
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "");                                                        /* nothing to commit */
    pac_textinput_pending_commit(&p, "line\nbreak");
    pac_textinput_pending_preedit(&p, "\x01", 0, 0);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]");                                               /* the preedit event still removes the old */
    char big[PAC_TEXTINPUT_TEXT_MAX + 10];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    s.log[0] = '\0';
    pac_textinput_pending_commit(&p, big);
    pac_textinput_pending_preedit(&p, big, 0, 0);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]");                                               /* too long: refused */
    big[PAC_TEXTINPUT_TEXT_MAX - 1] = '\0';                                      /* the longest that fits */
    s.log[0] = '\0';
    pac_textinput_pending_commit(&p, big);
    pac_textinput_apply(&p, &ops, &s);
    CHECK(strncmp(s.log, "P[,-1,-1]C[aaaa", 15) == 0);                            /* delivered (the log is shorter than it) */
    /* A cursor that does not fit the text is passed on as none. */
    s.log[0] = '\0';
    pac_textinput_pending_preedit(&p, "ab", 5, 1);
    pac_textinput_apply(&p, &ops, &s);
    CHECK_STR(s.log, "P[,-1,-1]P[ab,-1,-1]");
    /* A NULL sink or a NULL text is tolerated. */
    pac_textinput_pending_commit(&p, NULL);
    pac_textinput_pending_preedit(&p, NULL, 0, 0);
    pac_textinput_apply(&p, NULL, &s);
    pac_textinput_apply(&p, &ops, &s);
}

/* ---- layout: keyboard layouts, the picker and detecting the system's ---- */

static void test_layout_spec(void) {
    Layout l;
    CHECK(layout_parse_spec("us(intl)", &l));
    CHECK_STR(l.layout, "us"); CHECK_STR(l.variant, "intl"); CHECK_STR(l.name, "us(intl)");
    CHECK(layout_parse_spec("us (intl)", &l));           /* a space before the variant */
    CHECK_STR(l.layout, "us"); CHECK_STR(l.variant, "intl"); CHECK_STR(l.name, "us (intl)");
    CHECK(layout_parse_spec("pt   ", &l));                /* no trailing space in the name */
    CHECK_STR(l.layout, "pt"); CHECK_STR(l.variant, ""); CHECK_STR(l.name, "pt");
    CHECK(layout_parse_spec("  de(nodeadkeys)  ", &l));
    CHECK_STR(l.variant, "nodeadkeys"); CHECK_STR(l.name, "de(nodeadkeys)");
    CHECK(layout_parse_spec(" us(intl) ", &l));
    CHECK_STR(l.layout, "us"); CHECK_STR(l.variant, "intl");
    char spec[32];
    layout_spec(&l, spec, sizeof spec);
    CHECK_STR(spec, "us(intl)");
    CHECK(layout_parse_spec("pt", &l) && l.variant[0] == '\0');
    layout_spec(&l, spec, sizeof spec);
    CHECK_STR(spec, "pt");
    CHECK(!layout_parse_spec("", &l) && !layout_parse_spec("us(", &l) && !layout_parse_spec("us()", &l));
    CHECK(!layout_parse_spec("(intl)", &l) && !layout_parse_spec("   ", &l) && !layout_parse_spec("us(intl", &l));

    /* Too long for the fields: refused, never cut or overflowed. */
    char big[300];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    CHECK(!layout_parse_spec(big, &l));                   /* a layout code of 299 characters */
    char longvariant[400];
    snprintf(longvariant, sizeof longvariant, "us(%s)", big);
    CHECK(!layout_parse_spec(longvariant, &l));
    snprintf(longvariant, sizeof longvariant, "us(%.*s)", (int)sizeof l.variant, big);       /* too long for the variant's field */
    CHECK(!layout_parse_spec(longvariant, &l));
    snprintf(longvariant, sizeof longvariant, "us(%.*s)", (int)sizeof l.variant - 1, big);   /* the longest that fits */
    CHECK(layout_parse_spec(longvariant, &l) && strlen(l.variant) == sizeof l.variant - 1);
    snprintf(longvariant, sizeof longvariant, "us(intl)%s", big);                           /* the name stays inside its field */
    CHECK(layout_parse_spec(longvariant, &l) && strlen(l.name) < sizeof l.name);
}

static void test_layout_list(void) {
    Layout l[8];
    int n = layouts_parse_list("! model\n  pc105  Generic\n\n! layout\n  al              Albanian\n"
                               "  pt              Portuguese\n  us              English (US)\n\n"
                               "! variant\n  intl  us: English (US, intl.)\n", l, 8);
    CHECK(n == 3);
    CHECK_STR(l[0].layout, "al"); CHECK_STR(l[1].layout, "pt");
    CHECK_STR(l[1].name, "Portuguese");
    CHECK_STR(l[2].name, "English (US)");
    CHECK(l[2].variant[0] == '\0');
    /* the placeholder for a layout the user writes is not offered */
    CHECK(layouts_parse_list("! layout\n  us   English (US)\n  custom   A user-defined custom Layout\n", l, 8) == 1);
    /* only the "! layout" section, not another one that starts alike, or a variant's lines */
    CHECK(layouts_parse_list("! layouts\n  zz  Nowhere\n! variant\n  intl  x\n", l, 8) == 0);
    CHECK(layouts_parse_list("", l, 8) == 0 && layouts_parse_list("! layout\n", l, 8) == 0);
    /* hostile lists: the count never goes past max, a line longer than the buffer is cut, a name too long is cut */
    CHECK(layouts_parse_list("! layout\n  a A\n  b B\n  c C\n", l, 2) == 2);
    CHECK(layouts_parse_list("! layout\n  a A\n", l, 0) == 0);
    char list[1200] = "! layout\n  xx  ";
    memset(list + strlen(list), 'N', 600);
    strcpy(list + strlen(list), "\n  yy  Short\n");
    n = layouts_parse_list(list, l, 8);
    CHECK(n >= 1 && strlen(l[0].name) < sizeof l[0].name && strlen(l[0].layout) < sizeof l[0].layout);
    CHECK(layouts_parse_list("! layout\n  \t \n   \n  nameless\n", l, 8) == 1);
    CHECK_STR(l[0].name, "nameless");                       /* no description: the code is the name */

    /* configured layouts are described like the list describes them; this reads the system's list, if any */
    n = layouts_load(l, 8, "pt, us(intl)");
    CHECK(n == 2);
    CHECK_STR(l[0].layout, "pt"); CHECK_STR(l[1].variant, "intl");
    CHECK(layouts_load(l, 8, "pt, , (bad), us") == 2);       /* what cannot be read is skipped */
    CHECK(layouts_load(l, 1, "pt,us,de") == 1);
    CHECK(layouts_load(l, 8, NULL) <= 8);                    /* every layout the system knows, at most max */
    CHECK_STR(pac_layout_name("Not a layout"), "Not a layout");
    const char *portuguese = pac_layout_name("Portuguese");  /* from the system's xkeyboard-config, where it has the language */
    CHECK(portuguese && portuguese[0]);
}

static void test_picker(void) {
    static LayoutPicker p;
    memset(&p, 0, sizeof(p));
    const char *names[] = { "", "Arabic", "English (US)", "Portuguese", "Portuguese (Brazil)" };
    const char *codes[] = { "", "ara", "us", "pt", "br" };
    p.count = 5;
    for (int i = 0; i < p.count; i++) {
        snprintf(p.items[i].name, sizeof(p.items[i].name), "%s", names[i]);
        snprintf(p.items[i].layout, sizeof(p.items[i].layout), "%s", codes[i]);
    }

    /* without a search, left and right go through all of them, the compositor's first */
    CHECK(picker_step(&p, 1) == 1);
    p.current = 4;
    CHECK(picker_step(&p, 1) == 0);
    CHECK(picker_step(&p, -1) == 3);

    /* going round: from the last layout to the first, and back */
    p.current = 4;
    CHECK(picker_step(&p, 1) == 0);
    CHECK(picker_step(&p, -1) == 3);
    p.current = 0;
    CHECK(picker_step(&p, -1) == 4);

    /* a layout that cannot be made is stepped past, at the end too, so it still goes round */
    p.items[4].unavailable = true;
    p.current = 3;
    CHECK(picker_step(&p, 1) == 0);
    p.current = 0;
    CHECK(picker_step(&p, -1) == 3);
    CHECK(picker_match_count(&p) == 3);
    p.items[4].unavailable = false;
    p.current = 4;

    /* a search proposes, and changes nothing until settled */
    field_append(&p.filter, "por");
    picker_filter_changed(&p);
    CHECK(p.pick == 3 && p.current == 4 && picker_match_count(&p) == 2);
    CHECK(picker_step(&p, 1) == -1 && p.pick == 4);
    CHECK(picker_step(&p, 1) == -1 && p.pick == 3);   /* wraps among the matches */
    CHECK(picker_settle(&p) == 3);
    CHECK(p.filter.len == 0 && p.pick == 0);

    /* a code matches too, and the search ignores case */
    field_append(&p.filter, "BR");
    picker_filter_changed(&p);
    CHECK(p.pick == 4 && picker_match_count(&p) == 1);
    CHECK(picker_step(&p, 1) == -1 && p.pick == 4);   /* the only one */
    picker_cancel(&p);
    CHECK(p.pick == 0 && picker_settle(&p) == -1);

    /* nothing matches */
    field_append(&p.filter, "klingon");
    picker_filter_changed(&p);
    CHECK(p.pick == 0 && picker_match_count(&p) == 0 && picker_settle(&p) == -1);
    picker_cancel(&p);

    /* the compositor's own layout (0) has no name, so no search proposes it */
    CHECK(picker_matches(&p, 0) && picker_matches(&p, 1));   /* without a search, everything is let through */
    field_append(&p.filter, "a");
    CHECK(!picker_matches(&p, 0) && picker_matches(&p, 1));
    picker_cancel(&p);

    /* a picker with only the compositor's layout has nowhere to step */
    static LayoutPicker alone;
    memset(&alone, 0, sizeof alone);
    alone.count = 1;
    CHECK(picker_step(&alone, 1) == -1 && picker_match_count(&alone) == 0);
}

static void write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

static void test_layout_text(void) {
    /* The words are the English ones where nothing translates them, and the same function says them everywhere. */
    CHECK_STR(layout_text_hint(), "Type to search");
    CHECK_STR(layout_text_nothing(), "No keyboard matches");
    char out[64];
    CHECK_STR(layout_text_name(out, sizeof out, "Portuguese"), "Portuguese keyboard");
    CHECK(layout_text_name(out, sizeof out, "x") == out);
    /* A name that does not fit is cut, not overflowed. */
    char small[12];
    CHECK(layout_text_name(small, sizeof small, "A very long layout name") == small && strlen(small) == sizeof small - 1);
    CHECK(layout_text_name(small, 1, "x") == small && small[0] == '\0');
}

static void test_layout_detect(void) {
    Layout l;

    CHECK(layout_from_xorg("Section \"InputClass\"\n  Identifier \"system-keyboard\"\n"
                           "  MatchIsKeyboard \"on\"\n  Option \"XkbLayout\" \"pt,us\"\n"
                           "  Option \"XkbVariant\" \"nodeadkeys,\"\nEndSection\n", &l));
    CHECK_STR(l.layout, "pt");
    CHECK_STR(l.variant, "nodeadkeys");
    CHECK(!layout_from_xorg("Section \"InputClass\"\n  # Option \"XkbLayout\" \"fr\"\nEndSection\n", &l));
    CHECK(!layout_from_xorg("", &l));

    CHECK(layout_from_shell_vars("XKBMODEL=\"pc105\"\nXKBLAYOUT=\"de\"\nXKBVARIANT=\"\"\nXKBOPTIONS=\"\"\n", &l));
    CHECK_STR(l.layout, "de");
    CHECK(layout_from_shell_vars("XKBLAYOUT=us\nXKBVARIANT=intl\n", &l) && strcmp(l.variant, "intl") == 0);
    CHECK(!layout_from_shell_vars("KEYMAP=pt-latin9\n", &l));

    Layout known[3] = { {"al", "", "Albanian", false}, {"pt", "", "Portuguese", false}, {"gb", "", "English (UK)", false} };
    CHECK(layout_from_console_keymap("pt-latin9", known, 3, &l) && strcmp(l.layout, "pt") == 0);
    CHECK(layout_from_console_keymap("pt", known, 3, &l));
    CHECK(layout_from_console_keymap("uk", known, 3, &l) && strcmp(l.layout, "gb") == 0);
    CHECK(layout_from_console_keymap("dvorak", known, 3, &l) && strcmp(l.layout, "us") == 0 &&
          strcmp(l.variant, "dvorak") == 0);
    CHECK(!layout_from_console_keymap("klingon-x", known, 3, &l));

    /* hostile text: a value or a line far longer than its buffer is cut, and nothing is read past the end */
    char huge[2000];
    memset(huge, 'a', sizeof huge - 1);
    huge[sizeof huge - 1] = '\0';
    char text[2600];
    snprintf(text, sizeof text, "XKBLAYOUT=%s\n", huge);
    CHECK(layout_from_shell_vars(text, &l) && strlen(l.layout) < sizeof l.layout);
    snprintf(text, sizeof text, "Option \"XkbLayout\" \"%s\"\n", huge);
    CHECK(!layout_from_xorg(text, &l));                   /* the line is cut before its closing quote: refused */
    CHECK(!layout_from_xorg("Option \"XkbLayout\" \"unterminated\n", &l));
    CHECK(layout_from_shell_vars("XKBLAYOUT=\"unterminated", &l) && strcmp(l.layout, "unterminated") == 0);   /* up to the end of the line */
    CHECK(!layout_from_console_keymap("", known, 3, &l));

    /* the files, in order: Xorg configuration, then /etc/default/keyboard, then vconsole.conf */
    char etc[300], path[400];
    snprintf(etc, sizeof etc, "%s/etc", tmp);
    CHECK(mkdir(etc, 0700) == 0);
    snprintf(path, sizeof path, "%s/X11", etc);
    CHECK(mkdir(path, 0700) == 0);
    snprintf(path, sizeof path, "%s/X11/xorg.conf.d", etc);
    CHECK(mkdir(path, 0700) == 0);
    snprintf(path, sizeof path, "%s/default", etc);
    CHECK(mkdir(path, 0700) == 0);

    CHECK(!layout_detect(etc, known, 3, &l));
    CHECK(!layout_detect("/nonexistent/etc", known, 3, &l));

    snprintf(path, sizeof path, "%s/vconsole.conf", etc);
    write_text(path, "FONT=default8x16\nKEYMAP=pt-latin9\n");
    CHECK(layout_detect(etc, known, 3, &l) && strcmp(l.layout, "pt") == 0);

    snprintf(path, sizeof path, "%s/default/keyboard", etc);
    write_text(path, "XKBLAYOUT=\"gb\"\n");
    CHECK(layout_detect(etc, known, 3, &l) && strcmp(l.layout, "gb") == 0);

    snprintf(path, sizeof path, "%s/X11/xorg.conf.d/00-keyboard.conf", etc);
    write_text(path, "Section \"InputClass\"\n Option \"XkbLayout\" \"al\"\nEndSection\n");
    CHECK(layout_detect(etc, known, 3, &l) && strcmp(l.layout, "al") == 0);
    snprintf(path, sizeof path, "%s/X11/xorg.conf.d/notes.txt", etc);   /* only .conf files are read */
    write_text(path, "Option \"XkbLayout\" \"fr\"\n");
    CHECK(layout_detect(etc, known, 3, &l) && strcmp(l.layout, "al") == 0);
}

/* ---- utf8, language, atomic_file ---- */

static void test_utf8_plain(void) {
    CHECK(utf8_plain("", 0) && utf8_plain("abc", 3) && utf8_plain("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", 9));
    CHECK(!utf8_plain("a\nb", 3) && !utf8_plain("\t", 1) && !utf8_plain("\x1b", 1) && !utf8_plain("a\x7f", 2));   /* controls */
    CHECK(!utf8_plain("a\xFF", 2) && !utf8_plain("\xC3", 1) && !utf8_plain("\xE2\x82", 2));                   /* bad, cut short */
    CHECK(!utf8_plain("\xC0\x80", 2) && !utf8_plain("\xED\xA0\x80", 3) && !utf8_plain("\xF4\x90\x80\x80", 4));
    CHECK(utf8_plain("ab\xC3\xA9", 2));                                    /* only n bytes are looked at: not past them */
    CHECK(!utf8_plain("\xE2\x82\xAC", 2));                                /* a sequence cut by n is not whole */
    CHECK(utf8_plain("a\0b", 3) == false);                                 /* a NUL is a control character */
}

static void test_utf8(void) {
    uint32_t cp = 0;
    CHECK(utf8_decode("a", &cp) == 1 && cp == 'a');
    CHECK(utf8_decode("\xC3\xA9", &cp) == 2 && cp == 0xE9);
    CHECK(utf8_decode("\xE2\x82\xAC", &cp) == 3 && cp == 0x20AC);
    CHECK(utf8_decode("\xF0\x9F\x98\x80", &cp) == 4 && cp == 0x1F600);
    CHECK(utf8_decode("\xF4\x8F\xBF\xBF", &cp) == 4 && cp == 0x10FFFF);
    /* What is refused: a stray continuation byte, an overlong form, a surrogate, a code point past
     * U+10FFFF, and a sequence cut short by the end of the string (never read past it). */
    CHECK(utf8_decode("\x80", &cp) == 0);
    CHECK(utf8_decode("\xC0\x80", &cp) == 0);
    CHECK(utf8_decode("\xE0\x80\x80", &cp) == 0);
    CHECK(utf8_decode("\xED\xA0\x80", &cp) == 0);
    CHECK(utf8_decode("\xF4\x90\x80\x80", &cp) == 0);
    CHECK(utf8_decode("\xF8\x88\x80\x80\x80", &cp) == 0);
    CHECK(utf8_decode("\xE2\x82", &cp) == 0);
    CHECK(utf8_decode("\xC3", &cp) == 0);
}

static void test_language(void) {
    unsetenv("LANGUAGE");
    CHECK(pac_set_language(""));                  /* the environment's choice stays */
    CHECK(getenv("LANGUAGE") == NULL);
    /* Names that are not usable change nothing. */
    CHECK(!pac_set_language("pt BR"));
    CHECK(!pac_set_language("pt;rm"));
    CHECK(!pac_set_language("../x/y"));
    char big[PAC_LANGUAGE_MAX + 2];
    memset(big, 'a', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    CHECK(!pac_set_language(big));                /* one byte too long */
    big[PAC_LANGUAGE_MAX] = '\0';
    CHECK(pac_set_language(big) || getenv("LANGUAGE"));   /* the longest fits; a locale may be missing */
    unsetenv("LANGUAGE");
    CHECK(getenv("LANGUAGE") == NULL);
    CHECK(pac_set_language("C"));                 /* "C" always exists */
    CHECK_STR(getenv("LANGUAGE"), "C");
    unsetenv("LANGUAGE");
}

static void test_atomic_write(void) {
    char path[300], other[300], buf[64];
    snprintf(path, sizeof path, "%s/atomic.txt", tmp);
    CHECK(atomic_write(path, "one\n", 4));
    memset(buf, 0, sizeof buf);
    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, sizeof buf) == 4);
    close(fd);
    CHECK_STR(buf, "one\n");
    struct stat st;
    CHECK(stat(path, &st) == 0 && (st.st_mode & 0777) == 0600);

    /* Written again, it replaces the file whole; nothing is left behind in the folder. */
    CHECK(atomic_write(path, "2", 1));
    memset(buf, 0, sizeof buf);
    fd = open(path, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, sizeof buf) == 1);
    close(fd);
    CHECK_STR(buf, "2");
    CHECK(atomic_write(path, "", 0));             /* empty data is a valid file */
    CHECK(stat(path, &st) == 0 && st.st_size == 0);
    char left[300];
    snprintf(left, sizeof left, "%s/.atomic.txt.%ld", tmp, (long)getpid());
    CHECK(access(left, F_OK) != 0);

    /* A symbolic link left where the temporary file goes is replaced, not followed. */
    snprintf(other, sizeof other, "%s/victim.txt", tmp);
    CHECK(atomic_write(other, "safe", 4));
    CHECK(symlink(other, left) == 0);
    CHECK(atomic_write(path, "new", 3));
    memset(buf, 0, sizeof buf);
    fd = open(other, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, sizeof buf) == 4);
    close(fd);
    CHECK_STR(buf, "safe");

    /* A link at path itself is replaced by the file, and its target is untouched. */
    char link[300];
    snprintf(link, sizeof link, "%s/link.txt", tmp);
    CHECK(symlink(other, link) == 0);
    CHECK(atomic_write(link, "via", 3));
    memset(buf, 0, sizeof buf);
    fd = open(other, O_RDONLY);
    CHECK(fd >= 0 && read(fd, buf, sizeof buf) == 4);
    close(fd);
    CHECK_STR(buf, "safe");
    CHECK(lstat(link, &st) == 0 && S_ISREG(st.st_mode));

    /* Failures leave no temporary file and report why. */
    char missing[300];
    snprintf(missing, sizeof missing, "%s/no/such/folder/f", tmp);
    CHECK(!atomic_write(missing, "x", 1) && errno == ENOENT);
    CHECK(!atomic_write("", "x", 1));
    CHECK(!atomic_write("dir/", "x", 1));         /* no file name */
    char longpath[PAC_FILE_PATH_MAX + 8];
    memset(longpath, 'a', sizeof longpath - 1);
    longpath[0] = '/';
    longpath[sizeof longpath - 1] = '\0';
    CHECK(!atomic_write(longpath, "x", 1));
    /* A path with no slash goes to the current folder. */
    int here = open(".", O_RDONLY | O_DIRECTORY);
    CHECK(here >= 0 && chdir(tmp) == 0);
    CHECK(atomic_write("here.txt", "h", 1));
    CHECK(access("here.txt", F_OK) == 0);
    if (here >= 0 && fchdir(here) != 0) CHECK(0);
    if (here >= 0) close(here);
}

/* ---- random input: what matters is that the sanitizers stay quiet ---- */

static unsigned rng = 12345;
static unsigned rnd(void) { return rng = rng * 1103515245u + 12345u; }

static void test_random_input(void) {
    char path[300];
    snprintf(path, sizeof path, "%s/r.conf", tmp);
    static const char alphabet[] = "ab =#\n\t\0\x01\x7f\xc3\xa9\xff";
    struct seen s;
    for (int round = 0; round < 300; round++) {
        FILE *f = fopen(path, "wb");
        int len = (int)(rnd() % 3000);
        for (int i = 0; i < len; i++) fputc(alphabet[rnd() % (sizeof alphabet - 1)], f);
        fclose(f);
        s.count = 0;
        CHECK(pac_config_load_file("t", path, collect, &s) >= 0);
        pac_config_load_file("t", path, pac_setting_store, NULL);
    }
    pac_setting_clear();

    Field f;
    field_clear(&f);
    for (int i = 0; i < 20000; i++) {
        char text[8] = { 0 };
        for (int j = 0, n = (int)(rnd() % 7); j < n; j++) text[j] = (char)(rnd() % 256);
        switch (rnd() % 5) {
        case 0: field_append(&f, text); break;
        case 1: field_append_key(&f, text, 30 + rnd() % 3, 0); break;
        case 2: field_backspace(&f); break;
        case 3: field_retype(&f, translate, rnd() % 2 ? "a" : "b"); break;
        default: if (rnd() % 50 == 0) field_clear(&f);
        }
        if (!(f.len < FIELD_MAX && f.count <= f.len && f.text[f.len] == '\0')) { CHECK(0); break; }
    }
    field_clear(&f);
}

static void remove_tree(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "chmod -R u+rwx '%s' && rm -rf '%s'", path, path);
    if (system(cmd) != 0) fprintf(report, "could not remove %s\n", path);
}

static void test_length(void) {
    PacLength l;

    CHECK(pac_length_parse("12px", &l) && l.value == 12 && l.unit == PAC_LENGTH_PX);
    CHECK(pac_length_parse("3.5 MM", &l) && l.value == 3.5 && l.unit == PAC_LENGTH_MM);
    CHECK(pac_length_parse("0.25in", &l) && l.value == 0.25 && l.unit == PAC_LENGTH_IN);
    CHECK(pac_length_parse(".5pt", &l) && l.value == 0.5 && l.unit == PAC_LENGTH_PT);
    CHECK(pac_length_parse("40%", &l) && l.value == 40 && l.unit == PAC_LENGTH_PERCENT);
    l = (PacLength){ 7, PAC_LENGTH_PX };
    const char *bad[] = { "", "12", "px", "mm", "-3mm", "+3mm", "0mm", "nanmm", "infmm", "1e999mm", "1e9mm",
                          "3 m", "3mmm", "3mm x", "3%%", "3\tmm", "0x10px", "1e1mm", "1.2.3mm", " 3mm", NULL };
    for (int i = 0; bad[i]; i++) CHECK(!pac_length_parse(bad[i], &l));
    CHECK(!pac_length_parse(NULL, &l));
    CHECK(l.value == 7 && l.unit == PAC_LENGTH_PX);   /* untouched by every refusal */

    /* A 3840x2160 panel of 600 mm at scale 2: 6.4 buffer pixels per mm, 3.2 logical. */
    CHECK(pac_px_per_mm(600, 3840, 2) == 3.2);
    /* Nothing believable: 96 dpi, in logical pixels (at scale 2 half of 3.7795...). */
    double d96 = 96.0 / 25.4;
    CHECK(pac_px_per_mm(0, 1920, 1) == d96);
    CHECK(pac_px_per_mm(-5, 1920, 1) == d96);
    CHECK(pac_px_per_mm(500, 0, 1) == d96);
    CHECK(pac_px_per_mm(160, 1920, 1) == 12);                 /* 304 dpi is believed */
    CHECK(pac_px_per_mm(1, 1920, 1) == d96);                  /* 48768 dpi */
    CHECK(pac_px_per_mm(5000, 1920, 1) == d96);               /* under 10 dpi */
    CHECK(pac_px_per_mm(0, 1920, 2) == d96 / 2);
    CHECK(pac_px_per_mm(500, 1920, 0) == d96);                /* a scale below 1 is not one */
    CHECK(pac_px_per_mm(500, 1920, NAN) == d96);

    CHECK(pac_px_per_mm_known(600, 3840, 2) == 3.2);
    CHECK(pac_px_per_mm_known(0, 1920, 1) == 0 && pac_px_per_mm_known(1, 1920, 1) == 0);   /* a guess is not known */
    CHECK(pac_px_per_mm_known(500, 1920, 0.5) == 0 && pac_px_per_mm_known(500, 0, 1) == 0);

    double ppm = 4;
    CHECK(pac_length_px((PacLength){ 1, PAC_LENGTH_IN }, ppm, 1000) == 25.4 * 4);
    CHECK(pac_length_px((PacLength){ 12, PAC_LENGTH_PX }, ppm, 1000) == 12);
    CHECK(pac_length_px((PacLength){ 3, PAC_LENGTH_MM }, ppm, 1000) == 12);
    CHECK(pac_length_px((PacLength){ 72, PAC_LENGTH_PT }, ppm, 1000) == 25.4 * 4);
    CHECK(pac_length_px((PacLength){ 5, PAC_LENGTH_PERCENT }, ppm, 1000) == 50);

    PacLength lo = { 3, PAC_LENGTH_MM }, hi = { 10, PAC_LENGTH_MM };   /* 12 to 40 px */
    CHECK(pac_length_clamp(5, &lo, &hi, ppm, 1000) == 12);
    CHECK(pac_length_clamp(20, &lo, &hi, ppm, 1000) == 20);
    CHECK(pac_length_clamp(90, &lo, &hi, ppm, 1000) == 40);
    CHECK(pac_length_clamp(5, NULL, &hi, ppm, 1000) == 5);
    CHECK(pac_length_clamp(90, &lo, NULL, ppm, 1000) == 90);
    CHECK(pac_length_clamp(20, NULL, NULL, ppm, 1000) == 20);
    CHECK(pac_length_clamp(20, &hi, &lo, ppm, 1000) == 40);   /* min above max: the minimum wins */

    /* A maximum under 2 mm (8 px here) or a minimum over 40 mm (160 px) is a mistake, not a wish. */
    PacLength tiny = { 0.5, PAC_LENGTH_MM }, huge = { 500, PAC_LENGTH_MM };
    CHECK(pac_length_clamp(30, NULL, &tiny, ppm, 1000) == 8);
    CHECK(pac_length_clamp(3, NULL, &tiny, ppm, 1000) == 3);
    CHECK(pac_length_clamp(30, &huge, NULL, ppm, 1000) == 160);
    CHECK(pac_length_clamp(300, &huge, NULL, ppm, 1000) == 300);

    /* The parser does not follow LC_NUMERIC: a locale that writes "1,5" must still take "1.5".
     * Skipped where that locale is not generated (the parser is locale-independent by construction). */
    char saved[64];
    const char *current = setlocale(LC_NUMERIC, NULL);
    snprintf(saved, sizeof saved, "%s", current ? current : "C");
    if (setlocale(LC_NUMERIC, "de_DE.UTF-8")) {
        double scale = 0;
        CHECK(pac_length_parse("3.5mm", &l) && l.value == 3.5);
        CHECK(pac_text_scale_parse("1.5", &scale) && scale == 1.5);
    } else {
        fprintf(stderr, "skipped: no de_DE.UTF-8 locale to test LC_NUMERIC independence\n");
    }
    setlocale(LC_NUMERIC, saved);
}

static void test_text_scale(void) {
    double scale = 7;
    const char *good[] = { "0.5", "1", "1.0", "1.5", "2.5", "0.50", "02" };
    const double want[] = { 0.5, 1, 1, 1.5, 2.5, 0.5, 2 };
    for (size_t i = 0; i < sizeof good / sizeof *good; i++) {
        scale = 7;
        CHECK(pac_text_scale_parse(good[i], &scale) && scale == want[i]);
    }
    /* Everything else is refused, and the value is left alone: a number out of range or NaN (which
     * passes `x < min || x > max`), text after the number, a sign, an exponent, a comma, a space. */
    const char *bad[] = { "", "nan", "NaN", "inf", "-inf", "-1", "0", "0.49", "2.51", "9", "1.5x", "1e1", "1,5",
                          " 1.5", "1.5 ", "+1.5", ".", "abc", "0x1" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        scale = 7;
        CHECK(!pac_text_scale_parse(bad[i], &scale) && scale == 7);
    }
    CHECK(!pac_text_scale_parse(NULL, &scale));
}

static void test_palette(void) {
    char why[160];

    CHECK(pac_contrast((PacColor){ 1, 1, 1 }, (PacColor){ 0, 0, 0 }) > 20.9);
    CHECK(pac_contrast((PacColor){ 0.5, 0.5, 0.5 }, (PacColor){ 0.5, 0.5, 0.5 }) == 1.0);

    bool ok = pac_palette_contrast_ok(&PAC_PALETTE_NORMAL, why, sizeof why);
    if (!ok) fprintf(report, "normal palette: %s\n", why);
    CHECK(ok);
    ok = pac_palette_contrast_ok(&PAC_PALETTE_HIGH, why, sizeof why);
    if (!ok) fprintf(report, "high palette: %s\n", why);
    CHECK(ok);

    ok = pac_palette_meanings_apart(&PAC_PALETTE_NORMAL, why, sizeof why);
    if (!ok) fprintf(report, "normal palette: %s\n", why);
    CHECK(ok);
    ok = pac_palette_meanings_apart(&PAC_PALETTE_HIGH, why, sizeof why);
    if (!ok) fprintf(report, "high palette: %s\n", why);
    CHECK(ok);

    /* A palette that breaks the rules is refused, so the test above cannot pass by accident. */
    PacPalette bad = PAC_PALETTE_NORMAL;
    bad.label = bad.background;
    CHECK(!pac_palette_contrast_ok(&bad, why, sizeof why));
    bad = PAC_PALETTE_NORMAL;
    bad.error = bad.warn;
    CHECK(!pac_palette_meanings_apart(&bad, why, sizeof why));
}

static void test_speech(void) {
    char code[32];
    speech_language_code("pt_BR.UTF-8", code, sizeof(code)); CHECK_STR(code, "pt-BR");
    speech_language_code("pt_BR:pt", code, sizeof(code)); CHECK_STR(code, "pt-BR");
    speech_language_code("en@euro", code, sizeof(code)); CHECK_STR(code, "en");
    speech_language_code("C", code, sizeof(code)); CHECK_STR(code, "");
    speech_language_code("POSIX", code, sizeof(code)); CHECK_STR(code, "");
    speech_language_code("", code, sizeof(code)); CHECK_STR(code, "");
    speech_language_code(NULL, code, sizeof(code)); CHECK_STR(code, "");
    char tiny[4];
    speech_language_code("pt_BR", tiny, sizeof(tiny)); CHECK_STR(tiny, "pt-");   /* cut, never overrun */
    speech_language_code("pt", tiny, 0);                                         /* size 0: untouched */

    /* The tool's own language first, then the environment in the order of gettext's. */
    unsetenv("LANGUAGE"); unsetenv("LC_ALL"); unsetenv("LC_MESSAGES"); unsetenv("LANG");
    CHECK_STR(speech_locale(NULL), "");
    setenv("LANG", "fr_FR", 1); CHECK_STR(speech_locale(""), "fr_FR");
    setenv("LC_MESSAGES", "de_DE", 1); CHECK_STR(speech_locale(NULL), "de_DE");
    setenv("LC_ALL", "es_ES", 1); CHECK_STR(speech_locale(NULL), "es_ES");
    setenv("LANGUAGE", "pt_BR:pt", 1); CHECK_STR(speech_locale(NULL), "pt_BR:pt");
    CHECK_STR(speech_locale("it"), "it");
    unsetenv("LANGUAGE"); unsetenv("LC_ALL"); unsetenv("LC_MESSAGES"); unsetenv("LANG");

    /* Without a backend, or with no server, nothing is spoken and nothing breaks. */
    speech_say("hello");   /* not open */
    speech_say(NULL);
    speech_say_as(SPEECH_IMPORTANT, "hello");
    speech_say_as((SpeechPriority)99, "hello");
    speech_cancel();       /* nothing to stop */
    speech_finish(1000);   /* nothing to wait for: returns at once */
    speech_close();
}

static char heard[8][600];
static int heard_n;
static long long clock_ms;
static SpeechPriority heard_priority[8];
static void hear(SpeechPriority priority, const char *text) {
    if (heard_n < 8) { heard_priority[heard_n] = priority; snprintf(heard[heard_n++], sizeof heard[0], "%s", text); }
}
static long long fake_now(void) { return clock_ms; }

/* The focus moves fast: only where it stops is said, the lead only when it changes, the description later. */
static void test_speech_announce(void) {
    speech_test_hooks(hear, fake_now);
    heard_n = 0; clock_ms = 0;
    speech_announce(&(SpeechAnnounce){ .lead = "3 results.", .name = "Firefox", .desc = "Web browser", .settle_ms = 150, .desc_ms = 1000 });
    CHECK(speech_next_ms() == 150);
    clock_ms = 100; speech_tick(); CHECK(heard_n == 0);
    speech_announce(&(SpeechAnnounce){ .lead = "3 results.", .name = "Chrome", .desc = "Another browser", .settle_ms = 150, .desc_ms = 1000 });
    CHECK(speech_next_ms() == 150);                        /* a new call starts the wait again */
    clock_ms = 250; speech_tick();
    CHECK(heard_n == 1); CHECK_STR(heard[0], "3 results. Chrome");   /* Firefox was never said */
    CHECK(speech_next_ms() == 1000);
    clock_ms = 1250; speech_tick();
    CHECK(heard_n == 2); CHECK_STR(heard[1], "Another browser");
    CHECK(heard_priority[0] == SPEECH_TEXT && heard_priority[1] == SPEECH_NOTIFICATION);   /* the name matters, the description is a bonus */
    CHECK(speech_next_ms() == -1);

    /* The same lead is not said again; a moved focus is not described. */
    speech_announce(&(SpeechAnnounce){ .lead = "3 results.", .name = "Files", .desc = "Folders", .settle_ms = 0, .desc_ms = 1000 });
    clock_ms = 1300; speech_tick();
    CHECK(heard_n == 3); CHECK_STR(heard[2], "Files");
    speech_announce(&(SpeechAnnounce){ .lead = "1 result.", .name = "Files", .desc = "Folders", .settle_ms = 0, .desc_ms = 1000 });
    clock_ms = 1400; speech_tick();
    CHECK(heard_n == 4); CHECK_STR(heard[3], "1 result. Files");
    clock_ms = 9000; speech_tick(); CHECK(heard_n == 5); CHECK_STR(heard[4], "Folders");

    /* The same name twice is said once; an intro is said every time and before the lead. */
    speech_announce(&(SpeechAnnounce){ .lead = "1 result.", .name = "Files", .settle_ms = 0 });
    clock_ms = 9100; speech_tick(); CHECK(heard_n == 5);
    speech_announce(&(SpeechAnnounce){ .intro = "Hello.", .lead = "Home.", .name = "Files", .settle_ms = 0 });
    clock_ms = 9200; speech_tick(); CHECK(heard_n == 6); CHECK_STR(heard[5], "Hello. Home. Files");

    /* The priority is the tool's to choose for the name. */
    speech_announce(&(SpeechAnnounce){ .name = "Failed", .priority = SPEECH_IMPORTANT, .settle_ms = 0 });
    clock_ms = 9250; speech_tick(); CHECK(heard_n == 7 && heard_priority[6] == SPEECH_IMPORTANT);
    heard_n = 6;

    /* A part too long is cut, never overrun. */
    char big[1000];
    memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = '\0';
    speech_announce(&(SpeechAnnounce){ .name = big, .settle_ms = 0 });
    clock_ms = 9300; speech_tick(); CHECK(heard_n == 7 && strlen(heard[6]) == 255);

    /* Cancelling forgets what was planned: neither the name nor the description is said. */
    speech_announce(&(SpeechAnnounce){ .name = "Gone", .desc = "Too", .settle_ms = 100, .desc_ms = 100 });
    speech_cancel();
    CHECK(speech_next_ms() == -1);
    clock_ms = 20000; speech_tick(); CHECK(heard_n == 7);
    speech_announce(&(SpeechAnnounce){ .name = "Said", .desc = "Not this", .settle_ms = 0, .desc_ms = 100 });
    clock_ms = 20100; speech_tick(); CHECK(heard_n == 8);
    speech_cancel();                                       /* after the name: no description */
    clock_ms = 30000; speech_tick(); CHECK(heard_n == 8);
    speech_test_hooks(NULL, NULL);
}

int main(void) {
    snprintf(tmp, sizeof tmp, "/tmp/pacommon-test-XXXXXX");
    if (!mkdtemp(tmp)) { perror("mkdtemp"); return 2; }

    /* Messages about bad lines are expected; keep the output to the failures. */
    report = fdopen(dup(STDERR_FILENO), "w");
    if (!report || !freopen("/dev/null", "w", stderr)) return 2;

    test_config_file();
    test_config_path();
    test_config_load();
    test_config_bool();
    test_config_shared();
    test_pac_conf_example();
    test_setting();
    test_field();
    test_field_retype();
    test_field_edit_key();
    test_fontfind();
    test_font_dirs();
    test_config_guards();
    test_setting_guards();
    test_field_guards();
    test_fontfind_guards();
    test_palette();
    test_text_scale();
    test_length();
    test_speech();
    test_speech_announce();
    test_field_preedit();
    test_textinput();
    test_layout_spec();
    test_layout_list();
    test_picker();
    test_layout_text();
    test_layout_detect();
    test_utf8();
    test_utf8_plain();
    test_language();
    test_atomic_write();
    test_random_input();

    remove_tree(tmp);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
