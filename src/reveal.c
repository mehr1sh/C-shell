#include "reveal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>    //for getcwd()
#include <dirent.h>    //for opendir() and struct dirent, readdir(), closedir()
#include <sys/types.h> //for uid_t or pid_t
#include <ctype.h>
#include <limits.h>
#include "globals.h"

#define MAX_PATH_LEN 1024

// for qsort
int compare_strings(const void *a, const void *b)
{
    // cast void pointers back to char pointers and then dereference
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    // strcmp() for lexicographical comparison
    return strcmp(str_a, str_b);
}

void execute_reveal(char **args, const char *home_dir)
{
    bool show_all = false;
    bool line_by_line = false;
    char target_path[MAX_PATH_LEN];
    struct dirent *dir_entry;
    DIR *dir_stream;
    char **file_list = NULL;
    int file_count = 0;
    int path_count = 0;

    strcpy(target_path, "."); // default target path

    // Parse arguments starting from index 1 (index 0 is "reveal")
    for (int i = 1; args[i] != NULL; i++)
    {
        if (args[i][0] == '-')
        {
            // Parse flags like -a, -l, -la
            for (int j = 1; args[i][j] != '\0'; j++)
            {
                if (args[i][j] == 'a')
                    show_all = true;
                else if (args[i][j] == 'l')
                    line_by_line = true;
                else
                {
                    printf("reveal: Invalid Syntax!\n");
                    return;
                }
            }
        }
        else
        {
            // Non-flag argument: target path
            path_count++;
            if (path_count > 1)
            {
                printf("reveal: Invalid Syntax!\n");
                return;
            }

            if (strcmp(args[i], "~") == 0)
            {
                strncpy(target_path, home_dir, MAX_PATH_LEN - 1);
                target_path[MAX_PATH_LEN - 1] = '\0';
            }
            else if (strcmp(args[i], "-") == 0)
            {
                // Access the global prev_dir directly (it's declared in globals.h)
                if (strlen(prev_dir) == 0)
                {
                    printf("No such directory!\n");
                    return;
                }
                strcpy(target_path, prev_dir);
            }
            else
            {
                strncpy(target_path, args[i], MAX_PATH_LEN - 1);
                target_path[MAX_PATH_LEN - 1] = '\0';
            }
        }
    }

    // Handle relative paths by converting to absolute paths
    char abs_path[MAX_PATH_LEN];
    if (target_path[0] != '/')
    {
        // It's a relative path, get current directory first
        if (getcwd(abs_path, sizeof(abs_path)) == NULL)
        {
            printf("No such directory!\n");
            return;
        }

        // If target_path is not ".", append it to current directory
        if (strcmp(target_path, ".") != 0)
        {
            // Make sure we don't overflow the buffer
            size_t current_len = strlen(abs_path);
            size_t target_len = strlen(target_path);
            
            if (current_len + 1 + target_len >= MAX_PATH_LEN)
            {
                printf("No such directory!\n");
                return;
            }

            // Add separator if needed
            if (abs_path[current_len - 1] != '/')
            {
                strcat(abs_path, "/");
            }
            strcat(abs_path, target_path);
            strcpy(target_path, abs_path);
        }
        else
        {
            strcpy(target_path, abs_path);
        }
    }

    // Open the directory stream
    dir_stream = opendir(target_path);
    if (dir_stream == NULL)
    {
        printf("No such directory!\n");
        return;
    }

    // Read directory entries into a dynamic array
    while ((dir_entry = readdir(dir_stream)) != NULL)
    {
        // check for 'a' flag to include hidden files(start with '.')
        if (!show_all && dir_entry->d_name[0] == '.')
        {
            continue; // skip hidden files if the flag is not set
        }

        file_list = realloc(file_list, (file_count + 1) * sizeof(char *));
        if (file_list == NULL)
        {
            perror("realloc failed");
            closedir(dir_stream);
            // free allocated memory if realloc fails
            for (int i = 0; i < file_count; i++)
            {
                free(file_list[i]);
            }
            free(file_list);
            return;
        }

        // copy dir entry name into a new memory block
        // strdup allocated memory and copies string so we will have to free it later
        file_list[file_count] = strdup(dir_entry->d_name);
        file_count++;
    }

    closedir(dir_stream);

    // sort the file list
    // qsort has pointer to array, number of elements, and the size of each element
    qsort(file_list, file_count, sizeof(char *), compare_strings);

    // print the sorted list based on the -l flag
    for (int i = 0; i < file_count; i++)
    {
        printf("%s", file_list[i]);
        if (line_by_line)
        {
            printf("\n");
        }
        else
        {
            if (i < file_count - 1)
            {
                printf(" "); // print a space until the last entry
            }
        }

        // free memory allocated for each string
        free(file_list[i]);
    }

    if (!line_by_line && file_count > 0)
    {
        printf("\n");
    }

    free(file_list);
}
