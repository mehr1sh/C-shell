#include "signals.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static volatile sig_atomic_t sigchld_flag = 0;
pid_t fg_pid = -1;

static void handle_sigchld(int sig) {
    (void)sig;
    sigchld_flag = 1;
}

static void handle_sigint(int sig) {
    (void)sig;
    if (fg_pid > 0) kill(-fg_pid, SIGINT);
}

static void handle_sigtstp(int sig) {
    (void)sig;
    if (fg_pid > 0) kill(-fg_pid, SIGTSTP);
}

void install_signal_handlers(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);

    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}

bool process_signal_events(void) {
    if (!sigchld_flag) return false;
    sigchld_flag = 0;

    int saved_errno = errno;
    bool printed = false;

    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        job_t *job = jobs_find_by_pid(pid);
        if (job) {
            if (WIFEXITED(status)) {
                printf("[%d] %s with pid %d exited normally\n",
                       job->job_id, job->command, job->pid);
            } else if (WIFSIGNALED(status)) {
                printf("[%d] %s with pid %d terminated by signal %d\n",
                       job->job_id, job->command, job->pid, WTERMSIG(status));
            }
            fflush(stdout);
            jobs_remove_by_index(job - job_list);
            printed = true;
        }
        else if (WIFSTOPPED(status)) {
            jobs_mark_stopped(pid);
            printed = true;
        }
        else if (WIFCONTINUED(status)) {
            job = jobs_find_by_pid(pid);
            if (job) job->state = RUNNING;
            printed = true;
        }
    }

    errno = saved_errno;
    return printed;
}
