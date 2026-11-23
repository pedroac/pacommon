#ifdef PACOMMON_I18N
#include PACOMMON_I18N
#else
#define _(s) (s)
#endif
#include "icon_theme.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "paths.h"
#include "tunables.h"

#define THEMES_MAX 8
#define ROOTS_MAX (PATHS_MAX + 2)

static bool enabled = true;
static bool rtl;                    /* look for the right-to-left variants first (icons_set_direction) */
static bool svg_missing;           /* rsvg-convert is not installed: said once, and no more tries */
static int svg_budget, pending;
static char themes[THEMES_MAX][64];
static int theme_count;
static char roots[ROOTS_MAX][PATHS_LEN + 8];   /* <data>/icons and ~/.icons */
static int root_count;

typedef struct {
    char spec[96];
    int px;
    char path[PATHS_LEN * 2];      /* the SVG waiting for its turn; empty otherwise */
    cairo_surface_t *surface;
    bool symbolic, svg_waiting;
} Icon;
static Icon cache[ICON_CACHE];
static const char *program = "pac";      /* names the tool in the one message this module prints */
static int cache_used;

static bool exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static void add_theme(const char *name) {
    if (!name[0] || strlen(name) >= sizeof themes[0] || theme_count >= THEMES_MAX) return;
    for (int i = 0; i < theme_count; i++)
        if (strcmp(themes[i], name) == 0) return;
    strcpy(themes[theme_count++], name);
}

/* gtk-icon-theme-name from the GTK 3 settings, if the person has chosen one. */
static void read_gtk_theme(void) {
    char config[PATHS_LEN], path[PATHS_LEN * 2];
    if (!paths_config(config, sizeof config) || snprintf(path, sizeof path, "%s/gtk-3.0/settings.ini", config) >= (int)sizeof path) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "gtk-icon-theme-name", 19) != 0) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        char *v = eq + 1;
        v += strspn(v, " \t");
        v[strcspn(v, "\r\n")] = '\0';
        if (strchr(v, '/') == NULL) add_theme(v);       /* a name, never a path */
    }
    fclose(f);
}

void icons_init(bool theme_icons, const char *prog) {
    program = prog;
    enabled = theme_icons;
    if (!enabled) return;
    PathList list;
    char home[PATHS_LEN];
    paths_data(&list);
    root_count = 0;
    for (int i = 0; i < list.count && root_count < ROOTS_MAX; i++)
        snprintf(roots[root_count++], sizeof roots[0], "%s/icons", list.dir[i]);
    if (paths_home(home, sizeof home) && root_count < ROOTS_MAX)
        snprintf(roots[root_count++], sizeof roots[0], "%s/.icons", home);
    theme_count = 0;
    read_gtk_theme();
    add_theme("Adwaita");
    add_theme("AdwaitaLegacy");
    add_theme("breeze");
    add_theme("Papirus");
    add_theme("hicolor");
}

void icons_set_direction(bool rtl_now) {
    if (rtl_now == rtl) return;
    rtl = rtl_now;
    icons_free();
}

void icons_new_frame(void) {
    svg_budget = ICON_SVG_PER_FRAME;
    pending = 0;
}

bool icons_pending(void) { return pending > 0; }

void icons_free(void) {
    for (int i = 0; i < cache_used; i++)
        if (cache[i].surface) cairo_surface_destroy(cache[i].surface);
    cache_used = 0;
}


/* ---- finding the file ---- */

static const char *const contexts[] = { "apps", "places", "actions", "devices", "categories", "status", "legacy", "mimetypes" };

/* The folder sizes to try for an icon of px pixels: the first that is not smaller (a picture made
 * larger looks soft), then the SVG, which fits any, then the smaller ones from the largest. */
static int size_order(int px, const char **out, int max) {
    static const int sizes[] = { 24, 32, 48, 64, 96, 128, 256, 512 };
    static const char *const names[] = { "24x24", "32x32", "48x48", "64x64", "96x96", "128x128", "256x256", "512x512" };
    int n = 0, count = (int)(sizeof sizes / sizeof *sizes);
    for (int i = 0; i < count && n < max; i++) if (sizes[i] >= px) out[n++] = names[i];
    if (n < max) out[n++] = "scalable";
    for (int i = count - 1; i >= 0 && n < max; i--) if (sizes[i] < px) out[n++] = names[i];
    return n;
}

/* The file of an icon called `name` for px pixels: PNG in a size folder, SVG in scalable/. */
static bool find_named(const char *name, int px, char *path, size_t size, bool *symbolic) {
    const char *order[10];
    int orders = size_order(px, order, 10);
    char file[128];
    static const char *const variants[] = { "%s", "%s-symbolic" };
    for (int t = 0; t < theme_count; t++)
        for (int r = 0; r < root_count; r++) {
            char base[PATHS_LEN * 3];
            if (snprintf(base, sizeof base, "%s/%s", roots[r], themes[t]) >= (int)sizeof base || access(base, X_OK) != 0) continue;
            for (int v = 0; v < 2; v++) {
                snprintf(file, sizeof file, variants[v], name);
                if (v == 1)                              /* Adwaita: <theme>/symbolic/<context>/ */
                    for (size_t c = 0; c < sizeof contexts / sizeof *contexts; c++) {
                        int w = snprintf(path, size, "%s/symbolic/%s/%s.svg", base, contexts[c], file);
                        if (w < (int)size && exists(path)) goto found;
                    }
                for (int s = 0; s < orders; s++) {
                    bool scalable = strcmp(order[s], "scalable") == 0;
                    for (size_t c = 0; c < sizeof contexts / sizeof *contexts; c++) {
                        /* <theme>/<size>/<context>/ (hicolor, Adwaita) and <theme>/<context>/<size> (breeze, Papirus) */
                        const char *ext = scalable || v == 1 ? "svg" : "png";
                        const char *num = scalable ? "scalable" : order[s];
                        int w = snprintf(path, size, "%s/%s/%s/%s.%s", base, num, contexts[c], file, ext);
                        if (w < (int)size && exists(path)) goto found;
                        if (!scalable) {                 /* the number alone: 48, not 48x48 */
                            char shortn[8];
                            snprintf(shortn, sizeof shortn, "%d", atoi(order[s]));
                            w = snprintf(path, size, "%s/%s/%s/%s.%s", base, contexts[c], shortn, file, ext);
                            if (w < (int)size && exists(path)) goto found;
                        } else {
                            w = snprintf(path, size, "%s/%s/scalable/%s.svg", base, contexts[c], file);
                            if (w < (int)size && exists(path)) goto found;
                        }
                    }
                }
                continue;
            found:
                *symbolic = v == 1;
                return true;
            }
        }
    /* The folder old applications put their icons in. */
    static const char *const ext[] = { "png", "svg" };
    for (int e = 0; e < 2; e++) {
        snprintf(path, size, "/usr/share/pixmaps/%s.%s", name, ext[e]);
        if (exists(path)) {
            *symbolic = false;
            return true;
        }
    }
    return false;
}

/* One alternative of a spec: a path, or a name. Names with a '/' that are not absolute are refused. */
static bool find(const char *alt, int px, char *path, size_t size, bool *symbolic) {
    *symbolic = false;
    if (alt[0] == '/') {
        if (strlen(alt) >= size || !exists(alt)) return false;
        strcpy(path, alt);
        return true;
    }
    if (strchr(alt, '/') || strstr(alt, "..")) return false;
    if (rtl) {                                          /* its right-to-left variant, if the themes have one */
        char variant[128];
        size_t n = strlen(alt);
        bool already = n > 4 && (strcmp(alt + n - 4, "-rtl") == 0 || strcmp(alt + n - 4, "-ltr") == 0);
        if (!already && snprintf(variant, sizeof variant, "%s-rtl", alt) < (int)sizeof variant &&
            find_named(variant, px, path, size, symbolic))
            return true;
    }
    return find_named(alt, px, path, size, symbolic);
}

/* ---- making the picture ---- */

static cairo_surface_t *load_png(const char *path, int px) {
    cairo_surface_t *png = cairo_image_surface_create_from_png(path);
    int w = cairo_image_surface_get_width(png), h = cairo_image_surface_get_height(png);
    cairo_surface_t *s = NULL;
    if (cairo_surface_status(png) == CAIRO_STATUS_SUCCESS && w > 0 && h > 0 && w <= 4096 && h <= 4096) {
        s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px, px);
        cairo_t *cr = cairo_create(s);
        double k = (double)px / (w > h ? w : h);
        cairo_translate(cr, (px - w * k) / 2, (px - h * k) / 2);
        cairo_scale(cr, k, k);
        cairo_set_source_surface(cr, png, 0, 0);
        cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
    cairo_surface_destroy(png);
    return s;
}

static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000LL + t.tv_nsec / 1000000;
}

typedef struct { const unsigned char *p; size_t len, pos; } MemPng;

static cairo_status_t mem_png_read(void *closure, unsigned char *data, unsigned int n) {
    MemPng *m = closure;
    if (m->len - m->pos < n) return CAIRO_STATUS_READ_ERROR;
    memcpy(data, m->p + m->pos, n);
    m->pos += n;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *decode_output(const unsigned char *buf, size_t len, int px) {
    MemPng m = { buf, len, 0 };
    cairo_surface_t *img = cairo_image_surface_create_from_png_stream(mem_png_read, &m);
    int w = cairo_image_surface_get_width(img), h = cairo_image_surface_get_height(img);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS || w <= 0 || h <= 0) {
        cairo_surface_destroy(img);
        return NULL;
    }
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px, px);
    cairo_t *cr = cairo_create(s);
    double k = (double)px / (w > h ? w : h);
    cairo_translate(cr, (px - w * k) / 2, (px - h * k) / 2);
    cairo_scale(cr, k, k);
    cairo_set_source_surface(cr, img, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_destroy(img);
    return s;
}

/* SVG is drawn by rsvg-convert, run for the purpose and gone again, as pacbar does it: started
 * without a shell and with an absolute path, its PNG read on a pipe within a time and a size
 * limit; a tool that hangs or never stops writing is killed and the icon is missing. */
static cairo_surface_t *render_svg(const char *path, int px) {
    char dim[16];
    snprintf(dim, sizeof dim, "%d", px);
    char *argv[] = { "rsvg-convert", "-a", "-w", dim, "-h", dim, "-f", "png", (char *)path, NULL };
    static char *const clean_env[] = { (char *)"GIO_USE_VFS=local", NULL };

    int fds[2];
    if (path[0] != '/' || pipe2(fds, O_CLOEXEC) < 0) return NULL;
    sigset_t none, dfl;
    sigemptyset(&none);
    sigemptyset(&dfl);
    sigaddset(&dfl, SIGPIPE);
    sigaddset(&dfl, SIGCHLD);
    posix_spawnattr_t at;
    posix_spawnattr_init(&at);
    posix_spawnattr_setsigmask(&at, &none);
    posix_spawnattr_setsigdefault(&at, &dfl);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&fa, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    pid_t pid;
    int err = posix_spawnp(&pid, "rsvg-convert", &fa, &at, argv, clean_env);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    close(fds[1]);
    if (err) {
        if (err == ENOENT) {
            svg_missing = true;
            fprintf(stderr, _("%s: rsvg-convert is not installed, so SVG icons are not shown\n"), program);
        }
        close(fds[0]);
        return NULL;
    }

    static unsigned char buf[SVG_OUT_MAX];
    size_t len = 0;
    bool ok = true;
    long long deadline = now_ms() + SVG_WAIT_MS;
    for (;;) {
        long long left = deadline - now_ms();
        struct pollfd p = { .fd = fds[0], .events = POLLIN };
        int r = left > 0 ? poll(&p, 1, (int)left) : 0;
        if (r < 0 && errno == EINTR) continue;
        ssize_t n = r > 0 ? read(fds[0], buf + len, SVG_OUT_MAX - len) : -1;
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || (n > 0 && (len += (size_t)n) == SVG_OUT_MAX)) ok = false;   /* too late, or too much */
        if (n <= 0) break;
    }
    close(fds[0]);
    if (!ok || waitpid(pid, NULL, WNOHANG) == 0) {
        kill(pid, SIGKILL);
        waitpid(pid, NULL, 0);
    }
    return ok ? decode_output(buf, len, px) : NULL;
}

static bool is_svg(const char *path) {
    size_t n = strlen(path);
    return n > 4 && strcmp(path + n - 4, ".svg") == 0;
}

/* ---- the cache ---- */

IconState icon_get(const char *spec, int px, cairo_surface_t **surface, bool *symbolic) {
    *surface = NULL;
    *symbolic = false;
    if (!enabled || px <= 0 || px > 1024 || !spec[0] || strlen(spec) >= sizeof cache[0].spec) return ICON_MISSING;

    Icon *ic = NULL;
    for (int i = 0; i < cache_used; i++)
        if (cache[i].px == px && strcmp(cache[i].spec, spec) == 0) {
            ic = &cache[i];
            break;
        }
    if (!ic) {
        if (cache_used == ICON_CACHE) icons_free();     /* a long session of scrolling: start again */
        ic = &cache[cache_used++];
        memset(ic, 0, sizeof *ic);
        strcpy(ic->spec, spec);
        ic->px = px;
        char alts[sizeof ic->spec];
        strcpy(alts, spec);
        for (char *alt = alts, *next; alt; alt = next) {
            next = strchr(alt, '|');
            if (next) *next++ = '\0';
            char path[sizeof ic->path];
            bool sym;
            if (!find(alt, px, path, sizeof path, &sym)) continue;
            ic->symbolic = sym;
            if (is_svg(path)) {
                strcpy(ic->path, path);
                ic->svg_waiting = !svg_missing;
            } else {
                ic->surface = load_png(path, px);
            }
            if (ic->surface || ic->svg_waiting) break;
        }
    }
    if (ic->svg_waiting) {
        if (svg_budget == 0) {
            pending++;
            return ICON_PENDING;
        }
        svg_budget--;
        ic->surface = render_svg(ic->path, px);
        ic->svg_waiting = false;
    }
    if (!ic->surface) return ICON_MISSING;
    *surface = ic->surface;
    *symbolic = ic->symbolic;
    return ICON_READY;
}
