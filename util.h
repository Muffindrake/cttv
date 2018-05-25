#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define ARRSZ(arr) (sizeof (arr) / sizeof *(arr))
#define HCF(fmt, ...) \
        fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__, \
                        __VA_ARGS__)
#define HCF0(fmt) \
        fprintf(stderr, "%s:%d:%s(): " fmt "\n", __FILE__, __LINE__, __func__)
#define NPE(s) ((s) ? (s) : "nullptr "#s)

#define ERR_MEM "insufficient memory"

struct str_offs {
        char *data;
        char **offs;
        size_t data_len;
        size_t offs_len;
};

char *printma(const char *, ...);
char *xstrdup(const char *);
bool sys_session_graphical(void);
void local_readfile(const char *, struct str_offs *);
void local_free(struct str_offs *);
void murderize_single_quotes(char *);
char *request_single_sync(const char *, const char **);

#endif
