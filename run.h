#ifndef RUN_H
#define RUN_H

const char *run_mpv_ytdl(const char *, const char *);
const char *run_mpv_streamlink(const char *, const char *);
char *quality_ytdl(const char *);
char *quality_streamlink(const char *);

#endif
