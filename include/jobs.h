#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

typedef enum { RUNNING, STOPPED } job_state_t;

typedef struct {
    int job_id;
    pid_t pid;
    job_state_t state;
    char command[1024];
} job_t;

extern job_t job_list[1024];
extern int job_count;

// Job management
void jobs_add(pid_t pid, char *command);
void reap_background_children(void);
void jobs_mark_stopped(pid_t pid);
void jobs_ping(pid_t pid, int sig_num);
void jobs_print_activities(void);
void jobs_fg(int job_id);
void jobs_bg(int job_id);
pid_t get_foreground_pgid(void);

// Helpers
job_t *jobs_find_by_pid(pid_t pid);
job_t *jobs_find_by_id(int job_id);
void jobs_remove_by_index(int index);

#endif
