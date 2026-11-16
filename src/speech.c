#include "speech.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

#include <pthread.h>

static SPDConnection *conn;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ended = PTHREAD_COND_INITIALIZER;
static bool speaking;

/* Called by libspeechd's thread when a message ends or is cut off. */
static void on_end(size_t msg_id, size_t client_id, SPDNotificationType type) {
    (void)msg_id; (void)client_id; (void)type;
    pthread_mutex_lock(&lock);
    speaking = false;
    pthread_cond_broadcast(&ended);
    pthread_mutex_unlock(&lock);
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
    conn->callback_end = conn->callback_cancel = on_end;
    spd_set_notification_on(conn, SPD_END);
    spd_set_notification_on(conn, SPD_CANCEL);
    return true;
}

void speech_say(const char *text) {
    if (!conn || !set(text)) return;
    spd_cancel(conn);
    pthread_mutex_lock(&lock);
    speaking = true;
    pthread_mutex_unlock(&lock);
    if (spd_say(conn, SPD_MESSAGE, text) < 0) on_end(0, 0, SPD_EVENT_END);
}

void speech_finish(int timeout_ms) {
    if (!conn) return;
    struct timespec until;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += timeout_ms / 1000;
    until.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&lock);
    while (speaking && pthread_cond_timedwait(&ended, &lock, &until) == 0) {}
    pthread_mutex_unlock(&lock);
}

void speech_close(void) {
    if (conn) spd_close(conn);
    conn = NULL;
}
#else
bool speech_open(const char *tool, const char *language) { (void)tool; (void)language; return false; }
void speech_say(const char *text) { (void)text; }
void speech_finish(int timeout_ms) { (void)timeout_ms; }
void speech_close(void) {}
#endif
