#define _GNU_SOURCE
#include "cmdjob.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

long long job_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000LL + ts.tv_nsec / 1000000;
}

void job_init(Job *j, size_t cap) {
    j->buf = malloc(cap);
    j->cap = j->buf ? cap : 0;   /* with no room, the output is read and dropped */
    j->pid = 0;
    j->fd = -1;
    j->len = 0;
    j->deadline = 0;
}

void job_free(Job *j) {
    free(j->buf);
    j->buf = NULL;
    j->cap = 0;
}

#define ENV_MAX 512
static char *c_env[ENV_MAX + 2];
static char lc_all[] = "LC_ALL=C";
static bool c_env_done;

static void env_init(void) {
    if (c_env[0] || c_env_done) return;
    c_env_done = true;
    size_t n = 0;
    for (char **e = environ; e && *e && n < ENV_MAX; e++)
        if (strncmp(*e, "LC_ALL=", 7) != 0 && strncmp(*e, "LANGUAGE=", 9) != 0) c_env[n++] = *e;
    c_env[n++] = lc_all;
    c_env[n] = NULL;
}

void job_reset(Job *j) {
    j->pid = 0;
    j->fd = -1;
    j->len = 0;
    j->deadline = 0;
}

bool job_running(const Job *j) {
    return j->fd >= 0;
}

/* Runs argv (found in PATH) with `input` on its stdin (none: /dev/null) and its output, and its errors if
 * want_err, collected. Not a shell. */
bool job_start(Job *j, char *const argv[], const char *input, bool want_err, long long timeout_ms) {
    env_init();
    int out[2], in[2] = { -1, -1 };
    if (pipe2(out, O_CLOEXEC | O_NONBLOCK) < 0) return false;
    /* The reading end stays ours: the child gets the other one. A nonblocking write end is not wanted. */
    fcntl(out[1], F_SETFL, fcntl(out[1], F_GETFL) & ~O_NONBLOCK);
    if (input && pipe2(in, O_CLOEXEC) < 0) {
        close(out[0]);
        close(out[1]);
        return false;
    }

    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, out[1], 1);
    if (want_err) posix_spawn_file_actions_adddup2(&fa, out[1], 2);
    else posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    if (input) posix_spawn_file_actions_adddup2(&fa, in[0], 0);
    else posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);

    pid_t pid;
    int rc = posix_spawnp(&pid, argv[0], &fa, NULL, argv, c_env);
    posix_spawn_file_actions_destroy(&fa);
    close(out[1]);
    if (input) {
        close(in[0]);
        if (rc == 0) {
            size_t n = strlen(input);
            ssize_t w = write(in[1], input, n);
            if (w == (ssize_t)n) w = write(in[1], "\n", 1);
            (void)w;
        }
        close(in[1]);
    }
    if (rc != 0) {
        close(out[0]);
        return false;
    }
    j->pid = pid;
    j->fd = out[0];
    j->len = 0;
    j->deadline = timeout_ms > 0 ? job_now_ms() + timeout_ms : 0;
    return true;
}

/* Reads what is there. Returns 1 when the command has ended (its output is NUL-ended in buf, *ok says whether
 * it exited with 0), else 0. What does not fit in buf is read and dropped, so the command never blocks. */
int job_read(Job *j, bool *ok) {
    char tmp[1024];
    for (;;) {
        ssize_t n = read(j->fd, tmp, sizeof(tmp));
        if (n > 0) {
            size_t room = j->cap > 0 ? j->cap - 1 - j->len : 0;
            size_t take = (size_t)n < room ? (size_t)n : room;
            memcpy(j->buf + j->len, tmp, take);
            j->len += take;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
        break;
    }
    if (j->cap > 0) j->buf[j->len] = '\0';
    int status = 0;
    while (waitpid(j->pid, &status, 0) < 0 && errno == EINTR) {}
    *ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    close(j->fd);
    j->fd = -1;
    j->deadline = 0;
    return 1;
}

void job_kill(Job *j) {
    if (!job_running(j)) return;
    kill(j->pid, SIGKILL);
    close(j->fd);
    j->fd = -1;
    while (waitpid(j->pid, NULL, 0) < 0 && errno == EINTR) {}
    j->deadline = 0;
}


bool job_spawn_detached(char *const argv[]) {
    int ready[2];
    if (pipe2(ready, O_CLOEXEC) < 0) return false;
    pid_t first = fork();
    if (first < 0) {
        close(ready[0]);
        close(ready[1]);
        return false;
    }
    if (first == 0) {   /* the first child: it starts the program and is gone, so that the program belongs to init and nothing is left to wait for */
        setsid();
        pid_t second = fork();
        if (second != 0) _exit(second < 0 ? 1 : 0);
        int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, 0);
            dup2(null, 1);
            dup2(null, 2);
        }
        env_init();
        execvpe(argv[0], argv, c_env);
        char bad = 1;   /* it could not start: the pipe, which closes on a start, says so */
        if (write(ready[1], &bad, 1) < 0) {}
        _exit(127);
    }
    close(ready[1]);
    int status = 0;
    while (waitpid(first, &status, 0) < 0 && errno == EINTR) {}
    char c;
    ssize_t n;
    while ((n = read(ready[0], &c, 1)) < 0 && errno == EINTR) {}
    close(ready[0]);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && n == 0;
}
