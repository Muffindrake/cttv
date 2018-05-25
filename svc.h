#ifndef SVC_H
#define SVC_H

#include <stddef.h>
#include <stdint.h>

struct svc {
        void *handle;
        const char *name;
        const char *shrtname;
        const char *cfg_suf;
        const char *api_key;
        uint_least8_t ext_tool;

        void (*local_update)(struct svc *);
        const char *(*perform)(struct svc *);
        const char *(*stream_play)(const struct svc *, const char *, const char *);
        char *(*stream_quality)(const struct svc *, const char *);
        void (*cleanup)(struct svc *);
        size_t (*up_count)(const struct svc *);
        size_t (*total_count)(const struct svc *);
        char **(*chans)(const struct svc *);
        char **(*status)(const struct svc *);
        char **(*game)(const struct svc *);
        char **(*extra)(const struct svc *);
};

enum {
        SVCS_TWITCH,
        SVCS_AMNT
};

enum {
        EXT_TOOL_YTDL,
        EXT_TOOL_STREAMLINK,
        EXT_TOOL_AMNT
};

extern struct svc svcs[SVCS_AMNT];
extern const char *ext_tool[EXT_TOOL_AMNT];

#endif
