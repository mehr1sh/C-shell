#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

// using enum to define token types
typedef enum
{
    TOKEN_NAME,
    TOKEN_PIPE,
    TOKEN_AMPERSAND,
    TOKEN_DOUBLE_AMP,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_DOUBLE_GT,
    TOKEN_SEMICOLON,
    TOKEN_END
} token_type;

// global pointer to keep track of the current position in the input string
static char *pos;

// forward declarations
bool parse_shell_cmd();
bool parse_cmd_group();
bool parse_atomic();
bool parse_name();
bool parse_output();
bool parse_input_redirection(void);
void skip_whitespace();
token_type peek_next_token();
void consume_token();

bool is_valid_syntax(char *input)
{
    pos = input;
    bool parsing_successful = parse_shell_cmd();
    skip_whitespace();
    if (*pos != '\0')
    {
        return false;
    }
    return parsing_successful;
}

void skip_whitespace()
{
    while (*pos && isspace((unsigned char)*pos))
    {
        pos++;
    }
}

token_type peek_next_token()
{
    char *temp_pos = pos;
    skip_whitespace();
    token_type type;
    if (*pos == '\0')
    {
        type = TOKEN_END;
    }
    else if (*pos == '&' && *(pos + 1) == '&')
    {
        type = TOKEN_DOUBLE_AMP;
    }
    else if (*pos == '>' && *(pos + 1) == '>')
    {
        type = TOKEN_DOUBLE_GT;
    }
    else if (*pos == '|')
    {
        type = TOKEN_PIPE;
    }
    else if (*pos == '<')
    {
        type = TOKEN_LT;
    }
    else if (*pos == '>')
    {
        type = TOKEN_GT;
    }
    else if (*pos == '&')
    {
        type = TOKEN_AMPERSAND;
    }
    else if (*pos == ';')
    {
        type = TOKEN_SEMICOLON;
    }
    else
    {
        type = TOKEN_NAME;
    }
    pos = temp_pos;
    return type;
}

void consume_token()
{
    skip_whitespace();
    if (*pos == '\0')
        return;

    if (*pos == '&' && *(pos + 1) == '&')
    {
        pos += 2;
    }
    else if (*pos == '>' && *(pos + 1) == '>')
    {
        pos += 2;
    }
    else if (strchr("|&<>;", *pos))
    {
        pos++;
    }
    else
    {
        while (*pos && !isspace((unsigned char)*pos) && !strchr("|&<>;", *pos))
        {
            pos++;
        }
    }
}

bool parse_shell_cmd()
{
    if (!parse_cmd_group())
        return false;
    while (true)
    {
        token_type tok = peek_next_token();
        if (tok == TOKEN_DOUBLE_AMP || tok == TOKEN_SEMICOLON)
        {
            consume_token();
            if (!parse_cmd_group())
                return false;
        }
        else
        {
            break;
        }
    }
    if (peek_next_token() == TOKEN_AMPERSAND)
    {
        consume_token();
    }
    return true;
}

bool parse_cmd_group()
{
    if (!parse_atomic())
        return false;
    while (peek_next_token() == TOKEN_PIPE)
    {
        consume_token();
        token_type t = peek_next_token();
        if (t == TOKEN_PIPE || t == TOKEN_SEMICOLON || t == TOKEN_END)
            return false;
        if (!parse_atomic())
            return false;
    }
    return true;
}

bool parse_atomic()
{
    if (!parse_name())
        return false;
    while (true)
    {
        token_type nt = peek_next_token();
        if (nt == TOKEN_NAME)
        {
            if (!parse_name())
                return false;
        }
        else if (nt == TOKEN_LT)
        {
            if (!parse_input_redirection())
                return false;
        }
        else if (nt == TOKEN_GT || nt == TOKEN_DOUBLE_GT)
        {
            if (!parse_output())
                return false;
        }
        else
        {
            break;
        }
    }
    return true;
}

bool parse_name()
{
    if (peek_next_token() == TOKEN_NAME)
    {
        consume_token();
        return true;
    }
    return false;
}

command_t *parse_input(char *input)
{
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n')
    {
        input[len - 1] = '\0'; // Remove trailing newline
    }
    command_t *cmd = malloc(sizeof(command_t));
    if (!cmd)
        return NULL;
    cmd->argc = 0;
    cmd->background = false;
    cmd->in_files = calloc(16, sizeof(char *));
    cmd->in_count = 0;
    cmd->out_files = calloc(16, sizeof(char *));
    cmd->out_append = calloc(16, sizeof(bool));
    cmd->out_count = 0;
    if (!cmd->in_files || !cmd->out_files || !cmd->out_append)
    {
        free(cmd->in_files);
        free(cmd->out_files);
        free(cmd->out_append);
        free(cmd);
        return NULL;
    }
    cmd->argv = calloc(64, sizeof(char *));
    if (!cmd->argv)
    {
        free(cmd->in_files);
        free(cmd->out_files);
        free(cmd->out_append);
        free(cmd);
        return NULL;
    }

    char *copy = strdup(input);
    if (!copy)
    {
        free(cmd->argv);
        free(cmd->in_files);
        free(cmd->out_files);
        free(cmd->out_append);
        free(cmd);
        return NULL;
    }

    char *token, *saveptr;
    token = strtok_r(copy, " \t\n", &saveptr);
    while (token && cmd->argc < 63)
    {
        if (strcmp(token, "&") == 0)
        {
            cmd->background = true;
            token = strtok_r(NULL, " \t\n", &saveptr);
            continue;
        }
        if (token[0] == '<')
        {
            char *fname = token[1] ? strdup(token + 1) : strdup(strtok_r(NULL, " \t", &saveptr));
            cmd->in_files[cmd->in_count++] = fname;
        }
        else if (token[0] == '>')
        {
            bool append = token[1] == '>';
            char *fname = append
                              ? strdup(token[2] ? token + 2 : strtok_r(NULL, " \t", &saveptr))
                              : strdup(token[1] ? token + 1 : strtok_r(NULL, " \t", &saveptr));
            cmd->out_files[cmd->out_count] = fname;
            cmd->out_append[cmd->out_count++] = append;
        }
        else
        {
            cmd->argv[cmd->argc++] = strdup(token);
        }
        token = strtok_r(NULL, " \t\n", &saveptr);
    }
    cmd->argv[cmd->argc] = NULL;
    free(copy);

    if (cmd->argc > 0)
    {
        char *last = cmd->argv[cmd->argc - 1];
        size_t len = strlen(last);
        if (len && last[len - 1] == '&')
        {
            cmd->background = true;
            if (len == 1)
            {
                free(cmd->argv[--cmd->argc]);
                cmd->argv[cmd->argc] = NULL;
            }
            else
            {
                last[len - 1] = '\0';
            }
        }
    }
    return cmd;
}

bool parse_output()
{
    token_type nt = peek_next_token();
    if (nt != TOKEN_GT && nt != TOKEN_DOUBLE_GT)
        return false;
    consume_token();
    if (!parse_name())
    {
        while (*pos && !isspace((unsigned char)*pos))
            pos++;
        return false;
    }
    return true;
}

bool parse_input_redirection(void)
{
    if (peek_next_token() != TOKEN_LT)
        return false;
    consume_token();
    if (!parse_name())
    {
        while (*pos && !isspace((unsigned char)*pos))
            pos++;
        return false;
    }
    return true;
}

void free_command(command_t *cmd)
{
    if (!cmd)
        return;
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    for (int i = 0; i < cmd->in_count; i++)
        free(cmd->in_files[i]);
    free(cmd->in_files);
    for (int i = 0; i < cmd->out_count; i++)
        free(cmd->out_files[i]);
    free(cmd->out_files);
    free(cmd->out_append);
    free(cmd);
}
