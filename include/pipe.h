#ifndef PIPE_H
#define PIPE_H

#include <stdbool.h>
#include "parser.h"

void execute_pipeline(command_t **commands, bool background);void extract_redirection_and_cleanup(char **args, char **input_file, char **output_file, bool *append);

#endif // PIPE_H
