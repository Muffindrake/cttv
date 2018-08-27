#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>

#include <unistd.h>

#include "os.h"

int
os_chdir(const char *p)
{
        return !chdir(p);
}

bool
fexists(const char *p)
{
        return !access(p, F_OK);
}
