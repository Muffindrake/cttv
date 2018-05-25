#ifndef QUALITY_H
#define QUALITY_H

#include <stddef.h>

void quality_ytdl_populate(void);
void quality_ytdl_free(void);
char **quality_ytdl_arr(void);
size_t quality_ytdl_arrsz(void);

#endif
