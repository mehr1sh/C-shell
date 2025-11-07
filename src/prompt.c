#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  //for POSIX calls like getcwd()
#include <pwd.h>  //for password database like getpwuid
#include <sys/types.h>  //for data types like pid_t
#include <stdbool.h>

void display_prompt(const char *home_dir) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }

    // replace home_dir prefix with ~
    if (strncmp(cwd, home_dir, strlen(home_dir)) == 0) {
        printf("<mk@Mehrizm:~%s> ", cwd + strlen(home_dir));
    } else {
        printf("<mk@Mehrizm:%s> ", cwd);
    }

    fflush(stdout);
}
