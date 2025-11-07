#ifndef LOG_H
#define LOG_H

#include <stdbool.h>

#define MAX_HISTORY 15
#define MAX_CMD_LEN 1024

void add_to_log(char *cmd, const char *home_dir);
void execute_log(char **args, const char *home_dir, void (*run_command)(char*));

#endif