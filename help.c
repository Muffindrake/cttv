#include <stdio.h>

#include "help.h"
#include "svc.h"
#include "util.h"

void
help_print(void)
{
        size_t i;

        puts(HELPTEXT);

        puts("available support for stream services:");
        for (i = 0; i < SVCS_AMNT; i++)
                printf("%s:\tcfg_%s (%s)\n", NPE(svcs[i].name),
                                NPE(svcs[i].cfg_suf),
                                ext_tool[svcs[i].ext_tool]);
}
