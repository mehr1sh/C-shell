#include "hop.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include "globals.h"

void execute_hop(char **args, const char *home_dir) {
    char old_dir[PATH_MAX];
    if (getcwd(old_dir, sizeof(old_dir)) == NULL) {
        perror("getcwd failed");
        return;
    }

    // if no arguments, go home
    if (args[1] == NULL) {
        if (chdir(home_dir) == 0) {
            strncpy(prev_dir, old_dir, PATH_MAX);
            prev_dir[PATH_MAX - 1] = '\0';
        } else {
            printf("Cannot change to home directory!\n");
        }
        return;
    }

    // process arguments sequentially
    for (int i = 1; args[i] != NULL; i++) {
        char *target = args[i];
        char current_dir[PATH_MAX];
        
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("getcwd failed");
            return;
        }
        
        if (strcmp(target, "-") == 0) {
            if (strlen(prev_dir) == 0) {
                printf("No previous directory!\n");
                return;
            }
            target = prev_dir;
        } else if (strcmp(target, "~") == 0) {
            target = (char *)home_dir;
        }

        if (chdir(target) == 0) {
            strncpy(prev_dir, current_dir, PATH_MAX);
            prev_dir[PATH_MAX - 1] = '\0';
        } else {
            printf("No such directory!\n");
            return;
        }
    }
}
