#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "util.h"

char *
printma(const char *fmt, ...)
{
        va_list ap;
        char *ret;
        int len;

        va_start(ap, fmt);
        len = vsnprintf(0, 0, fmt, ap);
        va_end(ap);
        if (len < 0)
                return 0;
        if (len != INT_MAX)
                len++;
        ret = malloc(len);
        if (!ret)
                return 0;
        va_start(ap, fmt);
        len = vsnprintf(ret, len, fmt, ap);
        va_end(ap);
        if (len < 0) {
                free(ret);
                return 0;
        }
        return ret;
}

char *
xstrdup(const char *s)
{
        char *ret;

        ret = malloc(strlen(s) + 1);
        if (!ret)
                return 0;
        return strcpy(ret, s);
}

bool
sys_session_graphical(void)
{
        return getenv("DISPLAY");
}

void
local_readfile(const char *path, struct str_offs *loc)
{
        FILE *f;
        size_t i;

        f = fopen(path, "r");
        local_free(loc);
        if (!f)
                return;
        while (EOF != fscanf(f, "%*[^\n]") && EOF != fscanf(f, "%*c"))
                loc->offs_len++;
        if (!loc->offs_len)
                goto ret;
        loc->data_len = ftell(f);
        loc->data = malloc(loc->data_len + 1
                        + (loc->data_len + 1) % sizeof (void *)
                        + loc->offs_len * sizeof (void *));
        if (!loc->data) {
                loc->data_len = 0;
                loc->offs_len = 0;
                goto ret;
        }
        loc->offs = (char **) (loc->data + loc->data_len + 1
                        + (loc->data_len + 1) % sizeof (void *));
        *loc->offs = loc->data;
        rewind(f);
        i = fread(loc->data, 1, loc->data_len, f);
        loc->data[loc->data_len - 1] = 0;
        for (i = 1; i < loc->offs_len; i++) {
                loc->offs[i] = strchr(loc->offs[i - 1], '\n') + 1;
                loc->offs[i][-1] = 0;
        }
ret:
        fclose(f);
}

void
local_free(struct str_offs *loc)
{
        free(loc->data);
        *loc = (struct str_offs) {0};
}

void
murderize_single_quotes(char *s)
{
        for (; *s; s++) if (*s == '\'')
                *s = '_';
}

struct cb_data {
        char *p;
        size_t len;
};

static
size_t
callback_mem_write(void *data_p, size_t sz, size_t n, void *user_p)
{
        struct cb_data *data = user_p;
        size_t rsz = sz * n;

        data->p = realloc(data->p, data->len + rsz + 1);
        if (!data->p)
                return 0;
        memcpy(data->p + data->len, data_p, rsz);
        data->len += rsz;
        data->p[data->len] = 0;
        return rsz;
}

#define ERR_CURL_INIT "unable to initialise libcurl"

char *
request_single_sync(const char *url, const char **err)
{
        struct cb_data buf_ret;
        CURL *crl;
        CURLcode crlcode;

        buf_ret = (struct cb_data) {0};
        crl = curl_easy_init();
        if (!crl) {
                if (err)
                        *err = ERR_CURL_INIT;
                return 0;
        }
        curl_easy_setopt(crl, CURLOPT_URL, url);
        curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, callback_mem_write);
        curl_easy_setopt(crl, CURLOPT_WRITEDATA, &buf_ret);
        crlcode = curl_easy_perform(crl);
        curl_easy_cleanup(crl);
        if (crlcode != CURLE_OK) {
                if (err)
                        *err = curl_easy_strerror(crlcode);
                return 0;
        }
        return buf_ret.p;
}
