#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>   // for PATH_MAX

// a global array to store my job list
static int next_job_id = 1;

job_t job_list[1024];
int job_count = 0;

// a helper function to find a job by its pid
static job_t *find_job_by_pid(pid_t pid)
{
    for (int i = 0; i < job_count; i++)
    {
        if (job_list[i].pid == pid)
        {
            return &job_list[i];
        }
    }
    return NULL;
}

// a helper function to remove a job from the list when it is done
static void remove_job(int index)
{
    for (int i = index; i < job_count - 1; i++)
    {
        job_list[i] = job_list[i + 1];
    }
    job_count--;
}

// this function adds a new job to my job list
void jobs_add(pid_t pid, char *command)
{
    if (job_count >= 1024)
    {
        // no space left for more jobs
        return;
    }
    job_list[job_count].pid = pid;
    job_list[job_count].state = RUNNING;
    strcpy(job_list[job_count].command, command);
    job_list[job_count].job_id = next_job_id++;
    job_count++;
    // printf("[%d] %d\n", job_list[job_count - 1].job_id, pid);
}

void jobs_remove_by_index(int index)
{
    if (index < 0 || index >= job_count)
        return;
    for (int i = index; i < job_count - 1; i++)
    {
        job_list[i] = job_list[i + 1];
    }
    job_count--;
}

// a function to check for completed background children
void reap_background_children(void)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        job_t *job = find_job_by_pid(pid);
        if (job)
        {
            printf("[%d] %s with pid %d exited normally\n", job->job_id, job->command, job->pid);
            jobs_remove_by_index(job - job_list);
        }
    }
}

// a function to mark a job as stopped when it receives SIGTSTP
void jobs_mark_stopped(pid_t pid)
{
    job_t *job = find_job_by_pid(pid);
    if (job)
    {
        job->state = STOPPED;
        printf("\n[%d] Stopped %s\n", job->job_id, job->command);
    }
}

// a function to ping a job with a signal
void jobs_ping(pid_t pid, int sig_num)
{
    if (kill(pid, sig_num) == 0)
    {
        printf("Sent signal %d to process with pid %d\n", sig_num, pid);
    }
    else
    {
        perror("kill");
        printf("No such process found\n");
    }
}


// a function to print all the active jobs
int cmp(const void *a, const void *b)
{
    job_t *ja = (job_t *)a;
    job_t *jb = (job_t *)b;
    return strcmp(ja->command, jb->command);
}

void jobs_print_activities(void)
{
    qsort(job_list, job_count, sizeof(job_t), cmp);
    for (int i = 0; i < job_count; i++)
    {
        const char *state_str = (job_list[i].state == RUNNING) ? "Running" : "Stopped";
        printf("[%d] %d : %s - %s\n",
               job_list[i].job_id, job_list[i].pid,
               job_list[i].command, state_str);
    }
}

// a function to bring a job to the foreground
void jobs_fg(int job_id)
{
    // find the job
    for (int i = 0; i < job_count; i++)
    {
        if (job_list[i].job_id == job_id)
        {
            pid_t pid = job_list[i].pid;

            // tell the user what command is running
            printf("%s\n", job_list[i].command);

            extern pid_t fg_pid;
            fg_pid = pid;

            // set the job as the new foreground process group
            tcsetpgrp(STDIN_FILENO, pid);

            // send a SIGCONT to the job to make it run again if it was stopped
            if (job_list[i].state == STOPPED)
            {
                kill(-pid, SIGCONT);
            }

            // wait for the job to complete
            int status;
            do
            {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

            // move the shell back to the foreground
            tcsetpgrp(STDIN_FILENO, getpgrp());

            // remove the job from the list if it finished
            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                remove_job(i);
            }

            fg_pid = -1;

            return;
        }
    }

    printf("No such job\n");
}

// a function to move a stopped job to the background
void jobs_bg(int job_id)
{
    // find the job
    for (int i = 0; i < job_count; i++)
    {
        if (job_list[i].job_id == job_id)
        {
            if (job_list[i].state == RUNNING)
            {
                printf("Job already running\n");
                return;
            }
            // send the SIGCONT signal to the stopped job to make it run again
            job_list[i].state = RUNNING;
            kill(-job_list[i].pid, SIGCONT);
            printf("[%d] %s &\n", job_list[i].job_id, job_list[i].command);
            return;
        }
    }
    printf("No such job\n");
}

// a helper function to get the foreground pgid
pid_t get_foreground_pgid(void)
{
    return tcgetpgrp(STDIN_FILENO);
}

// helper: find job by pid (already have)
// add: find by job_id
job_t *jobs_find_by_pid(pid_t pid) {
  for (int i = 0; i < job_count; i++) {
    if (job_list[i].pid == pid) {
      return &job_list[i];
    }
  }
  return NULL;
}



job_t *jobs_find_by_id(int job_id)
{
    for (int i = 0; i < job_count; i++)
    {
        if (job_list[i].job_id == job_id)
            return &job_list[i];
    }
    return NULL;
}


