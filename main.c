#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "cfg.h"
#include "help.h"
#include "interface.h"
#include "os.h"
#include "quality.h"
#include "svc_active.h"
#include "util.h"

int
main(int argc, char **argv)
{
        (void) argv;

        setlocale(LC_ALL, "");
        if (argc >= 2) {
                help_print();
                return 0;
        }
        cfg_homeset();
        if (!os_chdir(cfg.cfg_home)) {
                HCF("unable to change working directory to: %s", cfg.cfg_home);
                return 1;
        }
        cfg_parse();
        cfg_keyset();
        quality_ytdl_populate();
        svc_populate();
        curl_global_init(CURL_GLOBAL_DEFAULT);
        nc_start();
        nc_loop_main();
        nc_end();
        curl_global_cleanup();
        free(cfg.x11_term);
        free(cfg.cfg_home);
        quality_ytdl_free();
        svc_free();
        return 0;
}
