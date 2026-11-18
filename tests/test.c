/* Unit tests for the shared code. None of it needs a display. */
#define _GNU_SOURCE
#include "config_file.h"
#include "field.h"
#include "fontfind.h"
#include "palette.h"
#include "setting.h"
#include "speech.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
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

static void test_config_shared(void) {
    CHECK(pac_config_shared_key("text_scale") && pac_config_shared_key("braille_table"));
    CHECK(!pac_config_shared_key("window_width") && !pac_config_shared_key("") && !pac_config_shared_key(NULL));

    char dir[300], cwd[300];
    snprintf(dir, sizeof dir, "%s/home2", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home2/pac", tmp);
    make_dir(dir);
    snprintf(dir, sizeof dir, "%s/home2/pac/pac.conf", tmp);
    write_file(dir, "font = Shared Sans\ntext_scale = 1.5\nspeech = yes\nlanguage = pt_BR\n"
                    "window_width = 100\ncontrast = high\ncontrast = yes\nlow_memory = maybe\n"
                    "text_scale_x = 2\n");
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
    CHECK(s.refused == 1);                                 /* speech was offered, and refused silently */
    for (int i = 0; i < s.count; i++) {                    /* not shared, or a bad value: never delivered */
        CHECK(strcmp(s.keys[i], "low_memory") != 0 && strcmp(s.keys[i], "text_scale_x") != 0);
        CHECK(!(strcmp(s.keys[i], "window_width") == 0 && strcmp(s.values[i], "100") == 0));
    }

    /* A tool that reads its own files only still gets the shared ones if it asks. */
    struct shared_seen only = { .count = 0 };
    pac_config_load_shared("tool", shared_collect, &only);
    CHECK_STR(last_value(&only, "font"), "Shared Sans");
    CHECK(last_value(&only, "window_width") == NULL);
    pac_config_load_shared("tool", NULL, NULL);            /* no handler: nothing to do, no crash */

    if (chdir(cwd) != 0) exit(2);
}

/* pac.conf, the example, says what the code accepts: every shared key is in it, and every line of
 * it, uncommented, is accepted (principle 23). */
static void test_pac_conf_example(void) {
    FILE *f = fopen("pac.conf", "r");
    if (!f) { fprintf(report, "FAIL pac.conf not found (run from the pacommon folder)\n"); failures++; return; }
    static const char *const keys[] = { "contrast", "text_scale", "font", "language", "icons", "theme",
                                        "speech", "low_memory", "braille_table" };
    int seen[9] = { 0 }, lines = 0;
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
        for (int i = 0; i < 9; i++) if (strcmp(key, keys[i]) == 0) seen[i]++;
    }
    fclose(f);
    fclose(o);
    CHECK(lines == 9);
    for (int i = 0; i < 9; i++) CHECK(seen[i] == 1 && pac_config_shared_key(keys[i]));

    /* Read as the shared file of a tool, whose values are checked first: all nine get through. */
    snprintf(dir, sizeof dir, "%s/home3", tmp);
    setenv("XDG_CONFIG_HOME", dir, 1);
    snprintf(dir, sizeof dir, "%s/home3/pac", tmp);   /* a folder with no pac.conf in it to stand in */
    if (!getcwd(cwd, sizeof cwd) || chdir(dir) != 0) { perror("chdir"); exit(2); }
    struct shared_seen s = { .count = 0 };
    pac_config_load_shared("t", collect_shared_example, &s);
    CHECK(s.count >= 9);                           /* a system pac.conf, if there is one, may add */
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
    test_config_shared();
    test_pac_conf_example();
    test_setting();
    test_field();
    test_field_retype();
    test_fontfind();
    test_font_dirs();
    test_config_guards();
    test_setting_guards();
    test_field_guards();
    test_fontfind_guards();
    test_palette();
    test_speech();
    test_speech_announce();
    test_random_input();

    remove_tree(tmp);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
