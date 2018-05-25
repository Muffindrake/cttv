#ifndef SVC_ACTIVE_H
#define SVC_ACTIVE_H

#include "svc.h"

struct svc *svc_arr(void);
size_t svc_arrsz(void);
void svc_populate(void);
void svc_free(void);

#endif
