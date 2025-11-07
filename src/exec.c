#include "exec.h"
#include "jobs.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>

pid_t current_fg_pid = -1;

// return 0 on success, -1 on failure (and print the required message)
int validate_redirections(command_t *cmd)
{
    //validate ALL input files (open & close — catches EACCES and ENOENT)
    for (int i = 0; i < cmd->in_count; ++i)
    {
        int fd = open(cmd->in_files[i], O_RDONLY);
        if (fd < 0)
        {
            // tests expect this exact string
            printf("No such file or directory\n");
            return -1;
        }
        close(fd);
    }

    //validate ALL output files. If any open fails, close previous fds and unlink
    //    only files we actually created during validation (don't unlink pre-existing files)
    if (cmd->out_count == 0)
        return 0;

    int outc = cmd->out_count;
    int *fds = calloc(outc, sizeof(int));
    bool *existed_before = calloc(outc, sizeof(bool));
    char **maybe_new_paths = calloc(outc, sizeof(char *));
    if (!fds || !existed_before || !maybe_new_paths)
    {
        free(fds);
        free(existed_before);
        free(maybe_new_paths);
        // on allocation failure, fail safe by refusing to run command
        printf("Unable to create file for writing\n");
        return -1;
    }

    struct stat st;
    int failed = 0;
    for (int i = 0; i < outc; ++i)
    {
        const char *p = cmd->out_files[i];
        existed_before[i] = (stat(p, &st) == 0);
        int flags = O_WRONLY | O_CREAT | (cmd->out_append[i] ? O_APPEND : O_TRUNC);
        int fd = open(p, flags, 0666);
        if (fd < 0)
        {
            failed = 1;
            break;
        }
        fds[i] = fd;
        // remember path only if we created it (so we can unlink it on failure)
        maybe_new_paths[i] = existed_before[i] ? NULL : strdup(p);
    }

    if (failed)
    {
        // close fds and unlink *only* those files we created during validation
        for (int j = 0; j < outc; ++j)
        {
            if (fds[j] > 0)
                close(fds[j]);
            if (maybe_new_paths[j])
            {
                unlink(maybe_new_paths[j]);
                free(maybe_new_paths[j]);
            }
        }
        free(fds);
        free(existed_before);
        free(maybe_new_paths);
        // tests expect this exact string
        printf("Unable to create file for writing\n");
        return -1;
    }

    // success: close validation fds and free memory (do NOT unlink)
    for (int i = 0; i < outc; ++i)
    {
        if (fds[i] > 0)
            close(fds[i]);
        if (maybe_new_paths[i])
        {
            free(maybe_new_paths[i]);
        }
    }
    free(fds);
    free(existed_before);
    free(maybe_new_paths);
    return 0;
}

void execute_command(command_t *cmd)
{
    if (!cmd || cmd->argc == 0)
        return;
        
    // validate redirections before fork
    if (validate_redirections(cmd) < 0)
    {
        return; // abort without fork
    }
    
    // Build full command string for job tracking
    char full_cmd[1024] = "";
    for (int i = 0; i < cmd->argc; i++) {
        if (i > 0) strcat(full_cmd, " ");
        strcat(full_cmd, cmd->argv[i]);
    }
    
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return;
    }
    
    if (pid == 0)
    {
        //child
        setpgid(0, 0);
        
        if (cmd->background)
        {
            int dn = open("/dev/null", O_RDONLY);
            if (dn >= 0)
            {
                dup2(dn, STDIN_FILENO);
                close(dn);
            }
        }
        
        // handle input redirection - last input wins
        if (cmd->in_count > 0)
        {
            char *infile = cmd->in_files[cmd->in_count - 1];
            int fd_in = open(infile, O_RDONLY);
            if (fd_in < 0)
            {
                printf("No such file or directory\n");
                exit(EXIT_FAILURE);
            }
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }
        
        // handle output redirection - last output wins
        if (cmd->out_count > 0)
        {
            char *outfile = cmd->out_files[cmd->out_count - 1];
            int flags = O_WRONLY | O_CREAT |
                       (cmd->out_append[cmd->out_count - 1] ? O_APPEND : O_TRUNC);
            int fd_out = open(outfile, flags, 0666);
            if (fd_out < 0)
            {
                printf("Unable to create file for writing\n");
                exit(EXIT_FAILURE);
            }
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }
        
        // exec the program
        if (execvp(cmd->argv[0], cmd->argv) == -1)
        {
            fprintf(stderr, "Command not found!\n");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        //parent
        if (!cmd->background)
        {
            setpgid(pid, pid);
            tcsetpgrp(STDIN_FILENO, pid);
            
            // set both fg_pid variables
            extern pid_t fg_pid;
            fg_pid = pid;
            current_fg_pid = pid;
            
            int status;
            do
            {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) &&
                    !WIFSIGNALED(status) &&
                    !WIFSTOPPED(status));
            tcsetpgrp(STDIN_FILENO, getpgrp());
            
            // clear both fg_pid variables
            fg_pid = -1;
            current_fg_pid = -1;
        }
        else
        {
            
            setpgid(pid, pid);
            jobs_add(pid, full_cmd);  // Use full command string instead of just cmd->argv[0]
        }
    }
}