#include "speech.h"
#include "tunables.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <time.h>

/* A tool that translates its messages names its i18n.h here (-DPACOMMON_I18N='"i18n.h"'). */
#ifdef PACOMMON_I18N
#include PACOMMON_I18N
#else
#define _(s) (s)
#endif

static bool set(const char *s) {
    return s && s[0];
}

const char *speech_locale(const char *language) {
    if (set(language)) return language;
    static const char *const names[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        const char *value = getenv(names[i]);
        if (set(value)) return value;
    }
    return "";
}

void speech_language_code(const char *locale, char *out, size_t size) {
    size_t n = 0;
    if (size == 0) return;
    out[0] = '\0';
    if (!locale) return;
    /* Only the first name of a list, and not its encoding or modifier. */
    for (const char *p = locale; *p && *p != ':' && *p != '.' && *p != '@' && n + 1 < size; p++)
        out[n++] = *p == '_' ? '-' : *p;
    out[n] = '\0';
    if (!strcmp(out, "C") || !strcmp(out, "POSIX") || !isalpha((unsigned char)out[0])) out[0] = '\0';
}

#ifdef SPEECH_SPEECHD
#include <libspeechd.h>
#include <stdatomic.h>

static SPDConnection *conn;
/* The message said last, and the last one the server reported as ended or cancelled. The callbacks
 * run in the library's own thread (SPD_MODE_THREADED). */
static atomic_size_t last_id, finished_id;

static void on_finished(size_t msg_id, size_t client_id, SPDNotificationType state) {
    (void)client_id; (void)state;
    atomic_store(&finished_id, msg_id);
}

bool speech_open(const char *tool, const char *language) {
    if (conn) return true;
    conn = spd_open(tool, "main", NULL, SPD_MODE_THREADED);
    if (!conn) {
        fprintf(stderr, _("%s: cannot reach the speech server, so nothing will be spoken\n"), tool);
        return false;
    }
    char code[32];
    speech_language_code(speech_locale(language), code, sizeof(code));
    if (code[0]) spd_set_language(conn, code);
    conn->callback_end = on_finished;
    conn->callback_cancel = on_finished;
    spd_set_notification_on(conn, SPD_END);
    spd_set_notification_on(conn, SPD_CANCEL);
    atomic_store(&last_id, 0);
    atomic_store(&finished_id, 0);
    return true;
}

static void say_spd(SPDPriority priority, const char *text) {
    int id = spd_say(conn, priority, text);
    atomic_store(&last_id, id > 0 ? (size_t)id : 0);
}

void speech_say(const char *text) {
    if (!conn || !set(text)) return;
    spd_cancel(conn);
    say_spd(SPD_MESSAGE, text);
}

void speech_say_as(SpeechPriority priority, const char *text) {
    static const SPDPriority spd[] = { [SPEECH_TEXT] = SPD_TEXT, [SPEECH_MESSAGE] = SPD_MESSAGE,
                                       [SPEECH_IMPORTANT] = SPD_IMPORTANT, [SPEECH_NOTIFICATION] = SPD_NOTIFICATION };
    if (!conn || !set(text) || (unsigned)priority >= sizeof spd / sizeof *spd) return;
    say_spd(spd[priority], text);
}

static void stop(void) {
    if (conn) spd_cancel(conn);
}

bool speech_busy(void) {
    size_t id = atomic_load(&last_id);
    return conn && id != 0 && atomic_load(&finished_id) != id;
}

void speech_finish(unsigned timeout_ms) {
    for (unsigned waited = 0; conn && waited < timeout_ms; waited += 20) {
        size_t id = atomic_load(&last_id);
        if (id == 0 || atomic_load(&finished_id) == id) return;
        nanosleep(&(struct timespec){ .tv_nsec = 20 * 1000 * 1000 }, NULL);
    }
}

void speech_close(void) {
    if (conn) spd_close(conn);
    conn = NULL;
}
#else
bool speech_open(const char *tool, const char *language) { (void)tool; (void)language; return false; }
void speech_say(const char *text) { (void)text; }
void speech_say_as(SpeechPriority priority, const char *text) { (void)priority; (void)text; }
static void stop(void) {}
bool speech_busy(void) { return false; }
void speech_finish(unsigned timeout_ms) { (void)timeout_ms; }
void speech_close(void) {}
#endif

/* ---- announcing the focus ---- */

static void (*say_hook)(SpeechPriority priority, const char *text);
static long long (*now_hook)(void);
/* What is planned: the parts, when the name is due and when the description is. */
static struct {
    char intro[PAC_SPEECH_PART_MAX], lead[PAC_SPEECH_PART_MAX], name[PAC_SPEECH_PART_MAX], desc[PAC_SPEECH_PART_MAX];
    bool name_pending, desc_pending;
    long long name_at, desc_at;
    unsigned desc_ms;
    SpeechPriority priority;
} plan;
static char last_lead[PAC_SPEECH_PART_MAX], last_name[PAC_SPEECH_PART_MAX];

void speech_cancel(void) {
    plan.name_pending = plan.desc_pending = false;
    stop();
}

static long long now(void) {
    if (now_hook) return now_hook();
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void say(SpeechPriority priority, const char *text) {
    if (say_hook) say_hook(priority, text);
    else speech_say_as(priority, text);
}

static bool is_busy(void) {
    return say_hook ? false : speech_busy();
}

static void copy(char *to, const char *from) {
    snprintf(to, PAC_SPEECH_PART_MAX, "%s", from ? from : "");
}

void speech_announce(const SpeechAnnounce *a) {
    copy(plan.intro, a->intro);
    copy(plan.lead, a->lead);
    copy(plan.name, a->name);
    copy(plan.desc, a->desc);
    plan.priority = a->priority;
    plan.name_pending = true;
    plan.desc_pending = false;      /* what is left behind is not described */
    plan.name_at = now() + a->settle_ms;
    plan.desc_ms = a->desc_ms;
}

int speech_next_ms(void) {
    long long at = -1;
    if (plan.name_pending) at = plan.name_at;
    else if (plan.desc_pending) at = plan.desc_at;
    if (at < 0) return -1;
    long long left = at - now();
    if (left <= 0 && !plan.name_pending && is_busy()) return 50;     /* the name is still being said: look again soon */
    return left <= 0 ? 0 : left > 1000000 ? 1000000 : (int)left;
}

void speech_tick(void) {
    if (plan.name_pending && now() >= plan.name_at) {
        plan.name_pending = false;
        bool lead_new = plan.lead[0] && strcmp(plan.lead, last_lead) != 0;
        char text[3 * PAC_SPEECH_PART_MAX + 2];
        snprintf(text, sizeof text, "%s%s%s%s%s", plan.intro, plan.intro[0] ? " " : "",
                 lead_new ? plan.lead : "", lead_new && plan.name[0] ? " " : "", plan.name);
        snprintf(last_lead, sizeof last_lead, "%s", plan.lead);
        bool name_new = strcmp(plan.name, last_name) != 0;
        snprintf(last_name, sizeof last_name, "%s", plan.name);
        if (text[0] && (plan.intro[0] || lead_new || name_new)) {
            say(plan.priority, text);
        }
        plan.desc_pending = plan.desc[0] != '\0';
        plan.desc_at = now() + plan.desc_ms;
    }
    if (plan.desc_pending && now() >= plan.desc_at && !is_busy()) {
        plan.desc_pending = false;
        say(SPEECH_NOTIFICATION, plan.desc);
    }
}

void speech_test_hooks(void (*say_fn)(SpeechPriority priority, const char *text), long long (*now_fn)(void)) {
    say_hook = say_fn;
    now_hook = now_fn;
    plan.name_pending = plan.desc_pending = false;
    last_lead[0] = last_name[0] = '\0';
}
