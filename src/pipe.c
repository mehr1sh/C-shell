#include "pipe.h"
#include "exec.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "globals.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

// helper function to check if command is a builtin
static int is_builtin(const char *cmd) {
    return (strcmp(cmd, "hop") == 0 ||
            strcmp(cmd, "reveal") == 0 ||
            strcmp(cmd, "log") == 0 ||
            strcmp(cmd, "echo") == 0);
}

// helper function to execute builtin commands in pipeline
static void execute_builtin_in_pipe(command_t *cmd, const char *home_dir) {
    if (strcmp(cmd->argv[0], "hop") == 0) {
        execute_hop(cmd->argv, home_dir);
    } else if (strcmp(cmd->argv[0], "reveal") == 0) {
        execute_reveal(cmd->argv, home_dir);
    } else if (strcmp(cmd->argv[0], "log") == 0) {
        // for log in pipeline, we need a dummy function pointer
        void dummy_run_command(char *c) { /* empty */ }
        execute_log(cmd->argv, home_dir, dummy_run_command);
    } else if (strcmp(cmd->argv[0], "echo") == 0) {
        for (int i = 1; cmd->argv[i] != NULL; i++) {
            printf("%s", cmd->argv[i]);
            if (cmd->argv[i + 1] != NULL)
                printf(" ");
        }
        printf("\n");
    }
}

void execute_pipeline(command_t **commands, bool background)
{
    if (!commands || !commands[0])
        return;
        
    int num_commands = 0;
    while (commands[num_commands] != NULL)
        num_commands++;
        
    pid_t pids[num_commands];
    int prev_read_fd = -1;
    
    for (int i = 0; i < num_commands; i++)
    {
        int pipefd[2] = {-1, -1};
        
        if (i < num_commands - 1)
        {
            if (pipe(pipefd) == -1)
            {
                perror("pipe failed");
                if (prev_read_fd != -1)
                    close(prev_read_fd);
                return;
            }
        }
        
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork failed");
            if (prev_read_fd != -1)
                close(prev_read_fd);
            if (pipefd[0] != -1)
                close(pipefd[0]);
            if (pipefd[1] != -1)
                close(pipefd[1]);
            return;
        }
        
        if (pid == 0)
        {
            // child
            setpgid(0, 0);
            
            // input from previous pipe
            if (prev_read_fd != -1)
            {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }
            
            // output to next pipe
            if (pipefd[1] != -1)
            {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }
            
            // input redirection (only for first command in pipeline)
            if (i == 0 && commands[i]->in_count > 0)
            {
                const char *infile = commands[i]->in_files[commands[i]->in_count - 1];
                int fd_in = open(infile, O_RDONLY);
                if (fd_in < 0)
                {
                    perror(infile);
                    _exit(EXIT_FAILURE);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            
            // output redirection (only for last command in pipeline)
            if (i == num_commands - 1 && commands[i]->out_count > 0)
            {
                const char *outfile = commands[i]->out_files[commands[i]->out_count - 1];
                bool append = commands[i]->out_append[commands[i]->out_count - 1];
                int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
                int fd_out = open(outfile, flags, 0644);
                if (fd_out < 0)
                {
                    perror(outfile);
                    _exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }
            
            // execute command - handle builtins specially
            if (is_builtin(commands[i]->argv[0]))
            {
                execute_builtin_in_pipe(commands[i], getenv("HOME"));
                _exit(EXIT_SUCCESS);
            }
            else
            {
                execvp(commands[i]->argv[0], commands[i]->argv);
                fprintf(stderr, "Command not found!\n");
                _exit(EXIT_FAILURE);
            }
        }
        
        // parent
        pids[i] = pid;
        
        if (prev_read_fd != -1)
            close(prev_read_fd);
            
        if (pipefd[0] != -1)
        {
            close(pipefd[1]);
            prev_read_fd = pipefd[0];
        }
    }
    
    if (prev_read_fd != -1)
        close(prev_read_fd);
        
    if (!background)
    {
        for (int i = 0; i < num_commands; i++)
        {
            int status;
            while (waitpid(pids[i], &status, 0) == -1 && errno == EINTR)
                ;
        }
    }
    else
    {
        // add all pipeline processes as background jobs
        for (int i = 0; i < num_commands; i++)
        {
            jobs_add(pids[i], commands[i]->argv[0]);
        }
    }
}