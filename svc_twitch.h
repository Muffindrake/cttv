#ifndef SVC_TWITCH_H
#define SVC_TWITCH_H

#include "svc.h"
#include "util.h"

void ttv_local_update(struct svc *);
const char *ttv_perform(struct svc *);
const char *ttv_stream_play(const struct svc *, const char *, const char *);
char *ttv_stream_quality(const struct svc *, const char *);
void ttv_cleanup(struct svc *);
size_t ttv_up_count(const struct svc *);
size_t ttv_total_count(const struct svc *);
char **ttv_chans(const struct svc *);
char **ttv_status(const struct svc *);
char **ttv_game(const struct svc *);

#endif
