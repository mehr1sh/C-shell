#ifndef GLOBALS_H
#define GLOBALS_H

#include <limits.h>
#include <unistd.h>  // ensures PATH_MAX is defined on POSIX

#ifndef PATH_MAX
#define PATH_MAX 4096   
#endif

extern char prev_dir[PATH_MAX];

#endif
