#include "quality.h"
#include "util.h"

static struct str_offs quality_ytdl;

void
quality_ytdl_populate(void)
{
        local_readfile("q_ytdl", &quality_ytdl);
}

void
quality_ytdl_free(void)
{
        local_free(&quality_ytdl);
}

char **
quality_ytdl_arr(void)
{
        return quality_ytdl.offs;
}

size_t
quality_ytdl_arrsz(void)
{
        return quality_ytdl.offs_len;
}
