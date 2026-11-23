#ifndef PACOMMON_CMDJOB_H
#define PACOMMON_CMDJOB_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* A command run without a shell, in the C locale, whose output is collected without ever blocking: the shared
 * way paconf talks to the programs it delegates to (nmcli, pactl). */

typedef struct {
    pid_t pid;
    int fd;                /* what the command writes; -1 when not running */
    char *buf;             /* its output, NUL-ended once it has ended; cap bytes at most are kept */
    size_t cap, len;
    long long deadline;    /* ms, 0 for none */
} Job;

long long job_now_ms(void);
/* Makes the room for `cap` bytes of output (1 or more). */
void job_init(Job *j, size_t cap);
void job_free(Job *j);
void job_reset(Job *j);
bool job_running(const Job *j);
/* Runs argv (found in PATH) with `input` on its stdin (none: /dev/null), and its output, and its errors if
 * want_err, collected. */
bool job_start(Job *j, char *const argv[], const char *input, bool want_err, long long timeout_ms);
/* Reads what is there. Returns 1 when the command has ended (*ok: it exited with 0), else 0. */
int job_read(Job *j, bool *ok);
void job_kill(Job *j);
/* Starts argv (found in PATH) as a program of its own: in a session of its own, with nothing on its standard streams, that goes on
 * when this one ends and that nothing here waits for. False if it could not be started. */
bool job_spawn_detached(char *const argv[]);

#endif
