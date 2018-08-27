#ifndef SVC_PICARTO_H
#define SVC_PICARTO_H

#include "svc.h"
#include "util.h"

void ptv_local_update(struct svc *);
const char *ptv_perform(struct svc *);
const char *ptv_stream_play(const struct svc *, const char *, const char *);
void ptv_cleanup(struct svc *);
size_t ptv_up_count(const struct svc *);
size_t ptv_total_count(const struct svc *);
char **ptv_chans(const struct svc *);

#endif
