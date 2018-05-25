#include <limits.h>
#include <stdlib.h>

#include <glib.h>
#include <ncurses.h>

#include "cfg.h"
#include "util.h"

struct cfg_s cfg;

void
cfg_homeset(void)
{
        const char *env;

        if (cfg.cfg_home)
                free(cfg.cfg_home);
        env = getenv("XDG_CONFIG_HOME");
        if (env) {
                cfg.cfg_home = printma("%s/cttv", env);
                if (!cfg.cfg_home) {
                        HCF0(ERR_MEM);
                        exit(1);
                }
                return;
        }
        env = getenv("HOME");
        if (env) {
                cfg.cfg_home = printma("%s/.config/cttv", env);
                if (!cfg.cfg_home) {
                        HCF0(ERR_MEM);
                        exit(1);
                }
                return;
        }
        HCF0("please set either XDG_CONFIG_HOME or HOME to a valid directory");
        exit(1);
}

#define CFG_GROUP "general"

void
cfg_parse(void)
{
        GKeyFile *cfgf;
        GError *err;
        char *val;

        err = 0;
        cfgf = g_key_file_new();
        if (!cfgf) {
                HCF0(ERR_MEM);
                exit(1);
        }
        if (!g_key_file_load_from_file(cfgf, "config", G_KEY_FILE_NONE, &err)) {
                HCF("unable to read default configuration file: %s",
                                err->message);
                exit(1);
        }
        val = g_key_file_get_string(cfgf, CFG_GROUP, "terminal", &err);
        if (!val) {
                HCF("unable to retrieve string from config: %s", err->message);
                exit(1);
        }
        cfg.refresh_timeout = g_key_file_get_integer(cfgf, CFG_GROUP,
                        "refresh_timeout", &err);
        if (err) {
                HCF("unable to read refresh timeout from config: %s",
                                err->message);
                exit(1);
        }
        if (cfg.refresh_timeout <= -1 || !cfg.refresh_timeout)
                cfg.refresh_timeout = -1;
        else if (cfg.refresh_timeout < 300)
                cfg.refresh_timeout = (300ull * 1000ull > INT_MAX
                        ? INT_MAX : 300 * 1000);
        else
                cfg.refresh_timeout = ((long long) cfg.refresh_timeout * 1000
                        > INT_MAX ? INT_MAX : cfg.refresh_timeout * 1000);
        cfg.x11_term = xstrdup(val);
        if (!cfg.x11_term) {
                HCF0(ERR_MEM);
                exit(1);
        }
        g_key_file_free(cfgf);
}

void
cfg_keyset(void)
{
        cfg.k_run = '\n';
        cfg.k_quit = 'Q';
        cfg.k_down = KEY_DOWN;
        cfg.k_down_vi = 'j';
        cfg.k_up = KEY_UP;
        cfg.k_up_vi = 'k';
        cfg.k_left = KEY_LEFT;
        cfg.k_left_vi = 'h';
        cfg.k_right = KEY_RIGHT;
        cfg.k_right_vi = 'l';
        cfg.k_home = KEY_HOME;
        cfg.k_end = KEY_END;
        cfg.k_update = 'r';
        cfg.k_quality_change_up = KEY_PPAGE;
        cfg.k_quality_change_down = KEY_NPAGE;
}
