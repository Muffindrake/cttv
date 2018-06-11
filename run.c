#include <stdlib.h>

#include <glib.h>

#include "cfg.h"
#include "run.h"
#include "util.h"

#define ERR_CMD "command could not be executed successfully"

const char *
run_mpv_ytdl(const char *url, const char *q)
{
        int ret;
        char *cmd;

        if (sys_session_graphical())
                cmd = printma("nohup %s -x \"mpv '%s' --ytdl-format='%s'\" "
                                ">/dev/null 2>&1 &", cfg.x11_term, url, q);
        else
                cmd = printma("mpv '%s' --ytdl-format='%s'", url, q);
        if (!cmd)
                return ERR_MEM;
        ret = system(cmd);
        free(cmd);
        if (ret < 0)
                return ERR_CMD;
        return 0;
}

const char *
run_mpv_streamlink(const char *url, const char *q)
{
        int ret;
        char *cmd;

        if (sys_session_graphical())
                cmd = printma("nohup %s -x \"streamlink --player=mpv '%s' '%s'\" "
                                ">/dev/null 2>&1 &", cfg.x11_term, url, q);
        else
                cmd = printma("streamlink --player=mpv '%s' '%s'", url, q);
        if (!cmd)
                return ERR_MEM;
        ret = system(cmd);
        free(cmd);
        if (ret < 0)
                return ERR_CMD;
        return 0;
}

char *
quality_ytdl(const char *url)
{
        char *argv[4];
        char *data;
        bool ret;

        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = printma("youtube-dl -F '%s' 2>/dev/null "
                        "| sed "
                        "-e '/^\\[.*/d' "
                        "-e '/^format code.*/d' "
                        "-e '/^ERROR.*/d' "
                        "| awk '{print $3, $4, $1}'", url);
        if (!argv[2])
                return 0;
        argv[3] = 0;
        ret = g_spawn_sync(0, argv, 0,
                        G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
                        0, 0, &data, 0, 0, 0);
        free(argv[2]);
        if (!ret)
                return 0;
        return data;
}

char *
quality_streamlink(const char *url)
{
        char *argv[4];
        char *data;
        bool ret;

        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = printma("streamlink -Q '%s' | cut -f3- -d ' '", url);
        if (!argv[2])
                return 0;
        argv[3] = 0;
        ret = g_spawn_sync(0, argv, 0,
                        G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_SEARCH_PATH,
                        0, 0, &data, 0, 0, 0);
        free(argv[2]);
        if (!ret)
                return 0;
        return data;
}
