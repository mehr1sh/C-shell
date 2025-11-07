#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <ctype.h>

// a function pointer which allows the execute_log to call back into the main loop's
// parsing/execution function without creating a circular dependency
void (*run_command_func)(char *);

// a small helper function I wrote to remove trailing whitespace from a string
void trim_whitespace(char *str)
{
    if (str == NULL)
        return;
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1]))
    {
        len--;
    }
    str[len] = '\0';
}

void add_to_log(char *cmd, const char *home_dir)
{
    char history[MAX_HISTORY][MAX_CMD_LEN];
    int count = 0;

    char history_path[MAX_CMD_LEN];
    snprintf(history_path, sizeof(history_path), "%s/.shell_history", home_dir);
    // snprintf prints formatted output to a string buffer instead of the screen

    // check if command is log and not to store it
    char temp_cmd[MAX_CMD_LEN];
    strcpy(temp_cmd, cmd);
    char *first_token = strtok(temp_cmd, " \t\n");

    if (first_token == NULL || strcmp(first_token, "log") == 0 || strcmp(first_token, "execute") == 0) {
        return;
    }
    if (first_token && strcmp(first_token, "log") == 0)
    {
        return;
    }

    if (first_token && strcmp(first_token, "execute") == 0)
    {
        return;
    }

    // read the existing history from the file
    FILE *log_file = fopen(history_path, "r");
    if (log_file)
    {
        while (fgets(history[count], MAX_CMD_LEN, log_file) && count < MAX_HISTORY)
        {
            // remove the newline char
            history[count][strcspn(history[count], "\n")] = 0;
            count++;
        }
        fclose(log_file);
    }

    // before we compare it to the history. This is a fix for the duplicate bug.
    trim_whitespace(cmd);

    // check duplicates
    if (count > 0 && strcmp(history[count - 1], cmd) == 0)
    {
        return;
    }

    // add new colog execmmand to the in memory array
    if (count < MAX_HISTORY)
    {
        strcpy(history[count], cmd);
        count++;
    }
    else
    {
        // full array; shift all commands up by one
        for (int i = 0; i < MAX_HISTORY - 1; i++)
        {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[MAX_HISTORY - 1], cmd);
    }

    // write the updated histroy back to the file
    log_file = fopen(history_path, "w");
    if (log_file)
    {
        for (int i = 0; i < count; i++)
        {
            fprintf(log_file, "%s\n", history[i]);
        }
        fclose(log_file);
    }
}

// this takes care of displaying, purging or executing a history command
void execute_log(char **args, const char *home_dir, void (*func)(char *))
{
    run_command_func = func;

    char history[MAX_HISTORY][MAX_CMD_LEN];
    int count = 0;
    char history_path[MAX_CMD_LEN];
    snprintf(history_path, sizeof(history_path), "%s/.shell_history", home_dir);

    // read existing history from file
    FILE *log_file = fopen(history_path, "r");
    if (log_file)
    {
        while (fgets(history[count], MAX_CMD_LEN, log_file) && count < MAX_HISTORY)
        {
            history[count][strcspn(history[count], "\n")] = 0;
            count++;
        }
        fclose(log_file);
    }

    // handdle log purge
    if (args[1] && strcmp(args[1], "purge") == 0)
    {
        // open the file in "w" mode to overwrite it with nothing- clear it
        log_file = fopen(history_path, "w");
        if (log_file)
        {
            fclose(log_file);
        }
        return;
    }

    // handle log execute <index>
    if (args[1] && strcmp(args[1], "execute") == 0)
    {
        if (!args[2])
        {
            printf("Usage: log execute <index>\n");
            return;
        }
        int index = atoi(args[2]);
        // assignment uses a 1-based, newest-to-oldest index
        // we convert this to our 0-based, oldest-to-newest index
        int array_index = count - index;
        if (array_index < 0 || array_index >= count)
        {
            printf("Invalid index.\n");
            return;
        }
        run_command_func(history[array_index]);

        return;
    }

    // handle "log" with no arguments
    for (int i = 0; i < count; i++)
    {
        printf("%s", history[i]);
        if (i < count - 1)
        {
            printf("\n");
        }
    }
    if (count > 0)
    {
        printf("\n"); // final newline
    }
}