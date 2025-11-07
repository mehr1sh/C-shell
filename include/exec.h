#ifndef EXEC_H
#define EXEC_H
#include "parser.h"

#include <stdbool.h>

int validate_redirections(command_t *cmd);
void execute_command(command_t *cmd);

#endif