#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "svc.h"
#include "svc_active.h"
#include "util.h"

static struct {
        struct svc *act;
        size_t n;
        size_t sz;
} svc_glob;

struct svc *
svc_arr(void)
{
        return svc_glob.act;
}

size_t
svc_arrsz(void)
{
        return svc_glob.n;
}

void
svc_populate(void)
{
        size_t i;

        if (svc_glob.act)
                return;
        svc_glob.n = SVCS_AMNT;
        svc_glob.sz = SVCS_AMNT;
        svc_glob.act = malloc(svc_glob.n * sizeof *svc_glob.act);
        if (!svc_glob.act) {
                HCF0(ERR_MEM);
                exit(1);
        }
        memcpy(svc_glob.act, svcs, sizeof svcs);
        for (i = 0; i < svc_glob.n; i++) if (svc_glob.act[i].local_update)
                svc_glob.act[i].local_update(svc_glob.act + i);
}

void
svc_free(void)
{
        size_t i;

        if (!svc_glob.act)
                return;
        for (i = 0; i < svc_glob.n; i++) if (svc_glob.act[i].cleanup)
                svc_glob.act[i].cleanup(svc_glob.act + i);
        free(svc_glob.act);
        svc_glob.act = 0;
}
