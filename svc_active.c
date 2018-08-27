#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
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
        char buf[1 << 6];
        size_t i;
        size_t init_counter;

        if (svc_glob.act)
                return;
#if 0
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
#endif
        svc_glob.n = 0;
        svc_glob.sz = ARRSZ(svcs);
        for (i = 0; i < ARRSZ(svcs); i++) {
                if (!svcs[i].cfg_suf)
                        continue;
                snprintf(buf, sizeof buf, "cfg_%s", svcs[i].cfg_suf);
                if (fexists(buf))
                        svc_glob.n++;
        }
        if (!svc_glob.n) {
                HCF0("No files for streaming services were found."
                                "It would be prudent to see the man page.");
                exit(1);
        }
        svc_glob.act = malloc(svc_glob.n * sizeof *svc_glob.act);
        if (!svc_glob.act) {
                HCF0(ERR_MEM);
                exit(1);
        }
        init_counter = 0;
        for (i = 0; i < ARRSZ(svcs); i++) {
                if (!svcs[i].cfg_suf)
                        continue;
                snprintf(buf, sizeof buf, "cfg_%s", svcs[i].cfg_suf);
                if (!fexists(buf))
                        continue;
                svc_glob.act[init_counter] = svcs[i];
                init_counter++;
        }
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
