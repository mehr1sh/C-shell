#include "prompt.h"
#include "parser.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "exec.h"
#include "pipe.h"
#include "config.h"
#include "jobs.h"
#include "signals.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>

#define MAX_ARGS 64
#define MAX_CMDS 10

// a small global variable to store the path for the home directory
char home_dir[1024];

// a helper function to encapsulate the parsing and execution logic
void run_command(char *cmd);

static void strip_surrounding_quotes(char *s)
{
    if (!s)
        return;
    size_t n = strlen(s);
    if (n >= 2)
    {
        if ((s[0] == '"' && s[n - 1] == '"') ||
            (s[0] == '\'' && s[n - 1] == '\''))
        {
            // remove the outer quotes in-place
            memmove(s, s + 1, n - 2);
            s[n - 2] = '\0';
        }
    }
}

// parse_arguments: reentrant tokenizer using strtok_r
void parse_arguments(char *input, char **args)
{
    char *token;
    char *saveptr;
    int i = 0;

    input[strcspn(input, "\n")] = '\0';

    token = strtok_r(input, " \t", &saveptr);
    while (token != NULL && i < MAX_ARGS - 1)
    {
        strip_surrounding_quotes(token);
        args[i++] = token;
        token = strtok_r(NULL, " \t", &saveptr);
    }
    args[i] = NULL;
}

int main(void)
{
    char input_buffer[INPUT_SIZE];
    if (getcwd(home_dir, sizeof(home_dir)) == NULL)
    {
        perror("getcwd");
        return 1;
    }

    prev_dir[0] = '\0';

    install_signal_handlers();

    while (1)
    {
        bool did_print = process_signal_events();

        // 2) Only show prompt if no exit message just printed
        if (!did_print)
        {
            display_prompt(home_dir);
        }

        // 3) Read user input
        if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL)
        {
            // EOF (Ctrl-D)
            for (int i = 0; i < job_count; i++)
            {
                kill(job_list[i].pid, SIGKILL);
            }
            printf("logout\n");
            exit(0);
        }

        // 5) Execute the command
        run_command(input_buffer);
    }

    return 0;
}

// helper: run a builtin with temporary redirections (foreground only)
static void run_builtin_with_redirs(
    void (*builtin_func)(char **, const char *),                    // hop
    void (*builtin_func2)(char **, const char *),                   // reveal
    void (*builtin_func3)(char **, const char *, void (*)(char *)), // log
    void (*simple_builtin)(char **),                                // echo
    char **args,
    char *in_file, char *out_file, bool append,
    const char *home_dir, char *prev_dir)
{
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);
    int fd;

    if (in_file)
    {
        fd = open(in_file, O_RDONLY);
        if (fd < 0)
        {
            fprintf(stderr, "No such file or directory\n");
            goto restore;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (out_file)
    {
        int flags = O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
        fd = open(out_file, flags, 0644);
        if (fd < 0)
        {
            fprintf(stderr, "No such file or directory\n");
            goto restore;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (builtin_func)
        builtin_func(args, home_dir);
    else if (builtin_func2)
        builtin_func2(args, home_dir);
    else if (builtin_func3)
        builtin_func3(args, home_dir, run_command);
    else if (simple_builtin)
        simple_builtin(args);

restore:
    if (saved_stdin >= 0)
    {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
    if (saved_stdout >= 0)
    {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
}

// builtin echo helper
static void builtin_echo(char **args)
{
    for (int i = 1; args[i] != NULL; i++)
    {
        printf("%s", args[i]);
        if (args[i + 1] != NULL)
            printf(" ");
    }
    printf("\n");
}

// the main command driver
void run_command(char *cmd)
{
    bool background = false;

    // at the beginning of run_command(), before any other processing:
    // handle "command & rest" pattern
    char *amp_space = strstr(cmd, " & ");
    if (amp_space && amp_space[3] != '\0')
    {                      // make sure there's something after " & "
        *amp_space = '\0'; // split the string
        // char *bg_part = cmd;
        char *fg_part = amp_space + 3;

        // execute background part
        // run_command(strcat(bg_part, " &")); // add & back for background processing
        if ((amp_space = strstr(cmd, " & ")) != NULL && *(amp_space + 3) != '\0')
        {
            *amp_space = '\0';
            char bg_cmd[1024];
            snprintf(bg_cmd, sizeof(bg_cmd), "%s &", cmd);

            run_command(bg_cmd);
            run_command(amp_space + 3);
            return;
        }

        // execute foreground part
        run_command(fg_part);
        return;
    }
    char job_cmd[1024];
    strncpy(job_cmd, cmd, sizeof(job_cmd) - 1);
    job_cmd[sizeof(job_cmd) - 1] = '\0';

    // size_t L = strlen(cmd);
    // while (L > 0 && isspace((unsigned char)cmd[L - 1]))
    //     L--;
    // if (L > 0 && cmd[L - 1] == '&')
    // {
    //     background = true;
    //     cmd[L - 1] = '\0';
    //     job_cmd[L - 1] = '\0';
    //     size_t K = strlen(job_cmd);
    //     while (K > 0 && isspace((unsigned char)job_cmd[K - 1]))
    //         job_cmd[--K] = '\0';
    // }

    char check_buf[1024];
    strncpy(check_buf, cmd, sizeof(check_buf) - 1);
    check_buf[sizeof(check_buf) - 1] = '\0';
    if (!is_valid_syntax(check_buf))
    {
        printf("Invalid Syntax!\n");
        return;
    }

    // handle sequential commands separated by ';'
    if (strchr(cmd, ';') != NULL)
    {
        char temp_seq_cmd[1024];
        strncpy(temp_seq_cmd, cmd, sizeof(temp_seq_cmd) - 1);
        temp_seq_cmd[sizeof(temp_seq_cmd) - 1] = '\0';

        char *saveptr_seq;
        char *seq_cmd = strtok_r(temp_seq_cmd, ";", &saveptr_seq);

        while (seq_cmd != NULL)
        {
            // trim leading/trailing spaces
            while (*seq_cmd && isspace((unsigned char)*seq_cmd))
                seq_cmd++;
            size_t len = strlen(seq_cmd);
            while (len > 0 && isspace((unsigned char)seq_cmd[len - 1]))
                seq_cmd[--len] = '\0';

            if (strlen(seq_cmd) > 0)
            {
                // recursively call run_command for each sequential command
                run_command(seq_cmd);
            }

            seq_cmd = strtok_r(NULL, ";", &saveptr_seq);
        }
        return;
    }

    // pipeline handling
    if (strchr(cmd, '|') != NULL)
    {
        command_t *commands[MAX_CMDS];
        int cmd_count = 0;

        char temp_pipe_cmd[1024];
        strncpy(temp_pipe_cmd, cmd, sizeof(temp_pipe_cmd) - 1);
        temp_pipe_cmd[sizeof(temp_pipe_cmd) - 1] = '\0';

        char *saveptr_pipe;
        char *cmd_str = strtok_r(temp_pipe_cmd, "|", &saveptr_pipe);
        while (cmd_str != NULL && cmd_count < MAX_CMDS)
        {
            // trim leading/trailing spaces
            while (*cmd_str && isspace((unsigned char)*cmd_str))
                cmd_str++;
            size_t len = strlen(cmd_str);
            while (len > 0 && isspace((unsigned char)cmd_str[len - 1]))
                cmd_str[--len] = '\0';

            command_t *c = parse_input(cmd_str);
            if (c)
                commands[cmd_count++] = c;

            cmd_str = strtok_r(NULL, "|", &saveptr_pipe);
        }
        commands[cmd_count] = NULL;

        execute_pipeline(commands, background);

        // cleanup
        for (int i = 0; i < cmd_count; ++i)
            free_command(commands[i]);

        return;
    }

    // single command
    command_t *parsed = parse_input(cmd);
    if (!parsed || parsed->argc == 0)
    {
        if (parsed)
            free_command(parsed);
        return;
    }

    background = parsed->background;

    // add to log (use original command, not parsed args)
    if (strcmp(parsed->argv[0], "log") != 0)
        add_to_log(job_cmd, home_dir);

    // builtins: hop, reveal, log, echo
    if (strcmp(parsed->argv[0], "hop") == 0)
    {
        if (!background)
        {
            // validate redirections first
            if (validate_redirections(parsed) < 0)
            {
                free_command(parsed);
                return;
            }
            run_builtin_with_redirs(execute_hop, NULL, NULL, NULL,
                                    parsed->argv,
                                    parsed->in_count > 0 ? parsed->in_files[parsed->in_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_files[parsed->out_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_append[parsed->out_count - 1] : false,
                                    home_dir, prev_dir);
        }
        else
        {
            // background hop - fork and handle redirections
            pid_t pid = fork();
            if (pid == 0)
            {
                setpgid(0, 0);
                if (parsed->in_count > 0)
                {
                    int fd = open(parsed->in_files[parsed->in_count - 1], O_RDONLY);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (parsed->out_count > 0)
                {
                    int flags = O_WRONLY | O_CREAT | (parsed->out_append[parsed->out_count - 1] ? O_APPEND : O_TRUNC);
                    int fd = open(parsed->out_files[parsed->out_count - 1], flags, 0644);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                execute_hop(parsed->argv, home_dir);
                _exit(EXIT_SUCCESS);
            }
            else if (pid > 0)
            {
                setpgid(pid, pid);
                jobs_add(pid, job_cmd);
            }
        }
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "reveal") == 0)
    {
        if (!background)
        {
            if (validate_redirections(parsed) < 0)
            {
                free_command(parsed);
                return;
            }
            run_builtin_with_redirs(NULL, execute_reveal, NULL, NULL,
                                    parsed->argv,
                                    parsed->in_count > 0 ? parsed->in_files[parsed->in_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_files[parsed->out_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_append[parsed->out_count - 1] : false,
                                    home_dir, prev_dir);
        }
        else
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                setpgid(0, 0);
                if (parsed->in_count > 0)
                {
                    int fd = open(parsed->in_files[parsed->in_count - 1], O_RDONLY);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (parsed->out_count > 0)
                {
                    int flags = O_WRONLY | O_CREAT | (parsed->out_append[parsed->out_count - 1] ? O_APPEND : O_TRUNC);
                    int fd = open(parsed->out_files[parsed->out_count - 1], flags, 0644);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                execute_reveal(parsed->argv, home_dir);
                _exit(EXIT_SUCCESS);
            }
            else if (pid > 0)
            {
                setpgid(pid, pid);
                jobs_add(pid, job_cmd);
            }
        }
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "log") == 0)
    {
        if (!background)
        {
            if (validate_redirections(parsed) < 0)
            {
                free_command(parsed);
                return;
            }
            run_builtin_with_redirs(NULL, NULL, execute_log, NULL,
                                    parsed->argv,
                                    parsed->in_count > 0 ? parsed->in_files[parsed->in_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_files[parsed->out_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_append[parsed->out_count - 1] : false,
                                    home_dir, prev_dir);
        }
        else
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                setpgid(0, 0);
                if (parsed->in_count > 0)
                {
                    int fd = open(parsed->in_files[parsed->in_count - 1], O_RDONLY);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (parsed->out_count > 0)
                {
                    int flags = O_WRONLY | O_CREAT | (parsed->out_append[parsed->out_count - 1] ? O_APPEND : O_TRUNC);
                    int fd = open(parsed->out_files[parsed->out_count - 1], flags, 0644);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                execute_log(parsed->argv, home_dir, run_command);
                _exit(EXIT_SUCCESS);
            }
            else if (pid > 0)
            {
                setpgid(pid, pid);
                jobs_add(pid, job_cmd);
            }
        }
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "echo") == 0)
    {
        if (!background)
        {
            if (validate_redirections(parsed) < 0)
            {
                free_command(parsed);
                return;
            }
            run_builtin_with_redirs(NULL, NULL, NULL, builtin_echo,
                                    parsed->argv,
                                    parsed->in_count > 0 ? parsed->in_files[parsed->in_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_files[parsed->out_count - 1] : NULL,
                                    parsed->out_count > 0 ? parsed->out_append[parsed->out_count - 1] : false,
                                    home_dir, prev_dir);
        }
        else
        {
            pid_t pid = fork();
            if (pid == 0)
            {
                setpgid(0, 0);
                if (parsed->in_count > 0)
                {
                    int fd = open(parsed->in_files[parsed->in_count - 1], O_RDONLY);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                if (parsed->out_count > 0)
                {
                    int flags = O_WRONLY | O_CREAT | (parsed->out_append[parsed->out_count - 1] ? O_APPEND : O_TRUNC);
                    int fd = open(parsed->out_files[parsed->out_count - 1], flags, 0644);
                    if (fd < 0)
                        _exit(EXIT_FAILURE);
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                builtin_echo(parsed->argv);
                _exit(EXIT_SUCCESS);
            }
            else if (pid > 0)
            {
                setpgid(pid, pid);
                jobs_add(pid, job_cmd);
            }
        }
        free_command(parsed);
        return;
    }

    // Part E Builtins
    if (strcmp(parsed->argv[0], "logout") == 0)
    {
        for (int i = 0; i < job_count; i++)
            kill(job_list[i].pid, SIGKILL);
        printf("logout\n");
        exit(0);
    }

    if (strcmp(parsed->argv[0], "activities") == 0)
    {
        jobs_print_activities();
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "ping") == 0)
    {
        if (!parsed->argv[1] || !parsed->argv[2])
        {
            printf("Usage: ping <pid> <signal_number>\n");
        }
        else
        {
            pid_t pid = (pid_t)atoi(parsed->argv[1]);
            int sig = atoi(parsed->argv[2]) % 32;
            jobs_ping(pid, sig);
        }
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "fg") == 0)
    {
        if (job_count == 0)
        {
            printf("No such job\n");
            free_command(parsed);
            return;
        }
        int job_num;
        if (parsed->argv[1])
        {
            job_num = atoi(parsed->argv[1]);
        }
        else
        {
            job_num = job_list[job_count - 1].job_id;
        }
        jobs_fg(job_num);
        free_command(parsed);
        return;
    }

    if (strcmp(parsed->argv[0], "bg") == 0)
    {
        if (!parsed->argv[1])
        {
            printf("Usage: bg <job_number>\n");
        }
        else
        {
            jobs_bg(atoi(parsed->argv[1]));
        }
        free_command(parsed);
        return;
    }

    // not builtin: external
    execute_command(parsed);
    free_command(parsed);
}