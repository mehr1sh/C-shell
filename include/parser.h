#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

typedef struct command {
    char **argv;
    int argc;
    bool background;

    char **in_files;
    int in_count;

    char **out_files;
    bool *out_append;   // true if ">>"
    int out_count;
} command_t;


// Updated return type
command_t *parse_input(char *input);

void free_command(command_t *cmd);
bool is_valid_syntax(char *input);
bool parse_input_redirection(void);

#endif
