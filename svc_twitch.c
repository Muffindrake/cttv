#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <curl/curl.h>
#include <glib.h>
#include <jansson.h>

#include "run.h"
#include "svc.h"
#include "svc_twitch.h"
#include "util.h"

struct ttv_s {
        struct str_offs local;
        char *data_name;
        char *data_game;
        char *data_status;
        char **offs_name;
        char **offs_game;
        char **offs_status;
        size_t up_cnt;
};

static
size_t
ttv_partition(struct svc *svc, size_t low, size_t high,
        int (*compare)(const char *, const char *))
{
        struct ttv_s *ttv = svc->handle;
        char *swp;
        size_t i;
        int res;

        i = low - 1;
        high++;
inf:
        do {
                i++;
                res = compare(ttv->offs_name[i], ttv->offs_name[low]);
        } while (res < 0);
        do {
                high--;
                res = compare(ttv->offs_name[high], ttv->offs_name[low]);
        } while (res > 0);
        if (i >= high)
                return high;
        swp = ttv->offs_name[i];
        ttv->offs_name[i] = ttv->offs_name[high];
        ttv->offs_name[high] = swp;
        swp = ttv->offs_game[i];
        ttv->offs_game[i] = ttv->offs_game[high];
        ttv->offs_game[high] = swp;
        swp = ttv->offs_status[i];
        ttv->offs_status[i] = ttv->offs_status[high];
        ttv->offs_status[high] = swp;
        goto inf;
}

static
void
ttv_quicksort(struct svc *svc, size_t low, size_t high,
        int (*compare)(const char *, const char *))
{
        size_t ret;

        if (low >= high)
                return;
        ret = ttv_partition(svc, low, high, compare);
        ttv_quicksort(svc, low, ret, compare);
        ttv_quicksort(svc, ret + 1, high, compare);
}

static
void
ttv_nonlocal_free(struct svc *svc)
{
        struct ttv_s *ttv = svc->handle;

        free(ttv->data_name);
        ttv->data_name = 0;
        ttv->data_game = 0;
        ttv->data_status = 0;
        ttv->offs_name = 0;
        ttv->offs_game = 0;
        ttv->offs_status = 0;
        ttv->up_cnt = 0;
}

#define TTVAPI \
"https://api.twitch.tv/helix/streams?first=100%s"
#define TTVAPI_USERNAME "&user_login="


static
char *
ttv_url_build(struct svc* svc)
{
        struct ttv_s *ttv = svc->handle;
        char *buf;
        char *ret;
        size_t i;
        size_t len;
        size_t len_left;
        size_t pos;
        int off;

        if (!ttv->local.offs_len)
                return 0;
        ret = 0;
        len = ttv->local.data_len + 1
                + ttv->local.offs_len * (sizeof TTVAPI_USERNAME - 1);
        buf = malloc(len);
        if (!buf)
                return 0;
        len_left = len - 1;
        pos = 0;
        for (i = 0; i < ttv->local.offs_len; i++) {
                off = snprintf(buf + pos, len_left, "%s%s",
                                TTVAPI_USERNAME, ttv->local.offs[i]);
                if (off <= 0)
                        goto fail;
                if (len_left && (size_t) off > len_left)
                        len_left = 0;
                else if (!len_left)
                        ;
                else
                        len_left -= off;
                if ((size_t) off > (len - 2) - pos)
                        pos += (len - 2) - pos;
                else
                        pos += off;
        }
        ret = printma(TTVAPI, buf);
fail:
        free(buf);
        return ret;
}

#define ERR_JSON_PARSE "unable to parse json"
#define ERR_JSON_ARRAY "unable to obtain array"
#define ERR_JSON_NOELEM "unable to obtain element from array"
#define ERR_JSON_NOCHANGAME "unable to obtain game_id from channel"
#define ERR_JSON_NOCHANNAME "unable to obtain user_name from channel"
#define ERR_JSON_NOCHANSTATUS "unable to obtain title from channel"

static
const char *
ttv_json_parse(struct svc *svc, const char *json)
{
        struct ttv_s *ttv = svc->handle;
        const char *err;
        size_t i;
        size_t offs_name;
        size_t offs_game;
        size_t offs_status;
        size_t offs_sum;
        json_t *root;
        json_t *streams;
        json_t *element;
        json_t *chan_name;
        json_t *chan_game;
        json_t *chan_status;

        err = 0;
        if (ttv->data_name)
                ttv_nonlocal_free(svc);
        root = json_loads(json, 0, 0);
        if (!root)
                return ERR_JSON_PARSE;
        streams = json_object_get(root, "data");
        if (!streams || JSON_ARRAY != json_typeof(streams)) {
                err = ERR_JSON_ARRAY;
                goto cleanup;
        }
        if (!(ttv->up_cnt = json_array_size(streams)))
                goto cleanup;
        offs_name = 0;
        offs_game = 0;
        offs_status = 0;
        for (i = 0; i < ttv->up_cnt; i++) {
                element = json_array_get(streams, i);
                if (!element) {
                        err = ERR_JSON_NOELEM;
                        goto cleanup;
                }
                chan_name = json_object_get(element, "user_name");
                if (!chan_name || json_typeof(chan_name) != JSON_STRING) {
                        err = ERR_JSON_NOCHANNAME;
                        goto cleanup;
                }
                offs_name += json_string_length(chan_name) + 1;
                chan_game = json_object_get(element, "game_id");
                if (!chan_game || json_typeof(chan_game) != JSON_STRING) {
                        err = ERR_JSON_NOCHANGAME;
                        goto cleanup;
                }
                offs_game += json_string_length(chan_game) + 1;
                chan_status = json_object_get(element, "title");
                if (!chan_status || json_typeof(chan_status) != JSON_STRING) {
                        err = ERR_JSON_NOCHANSTATUS;
                        goto cleanup;
                }
                offs_status += json_string_length(chan_status) + 1;
        }
        offs_sum = offs_name + offs_game + offs_status;
        ttv->data_name = malloc(offs_sum + offs_sum % sizeof (void *)
                        + sizeof (void *) * 3 * ttv->up_cnt);
        if (!ttv->data_name) {
                err = ERR_MEM;
                goto cleanup;
        }
        ttv->data_game = ttv->data_name + offs_name;
        ttv->data_status = ttv->data_game + offs_game;
        ttv->offs_name = (char **)(ttv->data_status + offs_status
                        + offs_sum % sizeof (void *));
        ttv->offs_game = ttv->offs_name + ttv->up_cnt;
        ttv->offs_status = ttv->offs_game + ttv->up_cnt;
        offs_name = 0;
        offs_game = 0;
        offs_status = 0;
        for (i = 0; i < ttv->up_cnt; i++) {
                element = json_array_get(streams, i);
                chan_name = json_object_get(element, "user_name");
                ttv->offs_name[i] = ttv->data_name + offs_name;
                strcpy(ttv->offs_name[i], json_string_value(chan_name));
                murderize_single_quotes(ttv->offs_name[i]);
                offs_name += json_string_length(chan_name) + 1;
                chan_game = json_object_get(element, "game_id");
                ttv->offs_game[i] = ttv->data_game + offs_game;
                strcpy(ttv->offs_game[i], json_string_value(chan_game));
                offs_game += json_string_length(chan_game) + 1;
                chan_status = json_object_get(element, "title");
                ttv->offs_status[i] = ttv->data_status + offs_status;
                strcpy(ttv->offs_status[i], json_string_value(chan_status));
                offs_status += json_string_length(chan_status) + 1;
        }
        if (ttv->up_cnt > 1)
                ttv_quicksort(svc, 0, ttv->up_cnt - 1, strcasecmp);
cleanup:
        json_decref(root);
        return err;
}

void
ttv_local_update(struct svc *svc)
{
        struct ttv_s *ttv;
        char *cfg_path;

        if (svc->handle)
                ttv_cleanup(svc);
        free(svc->handle);
        svc->handle = malloc(sizeof *ttv);
        if (!svc->handle)
                return;
        ttv = svc->handle;
        *ttv = (struct ttv_s) {0};
        cfg_path = printma("cfg_%s", svc->cfg_suf);
        if (!cfg_path)
                return;
        local_readfile(cfg_path, &ttv->local);
        free(cfg_path);
}

const char *
ttv_perform(struct svc *svc)
{
        const char *err;
        char *url;
        char *auth;
        struct curl_cb_data buf_ret;
        struct curl_slist *list;
        CURL *crl;
        CURLcode crlcode;

        if (!svc->api_key)
                return "No Twitch client ID in configuration";
        buf_ret = (struct curl_cb_data) {0};
        err = 0;
        list = 0;
        auth = 0;
        crl = 0;
        url = ttv_url_build(svc);
        if (!url)
                return ERR_MEM;
        auth = printma("Client-ID: %s", svc->api_key);
        if (!auth) {
                err = ERR_MEM;
                goto fail;
        }
        crl = curl_easy_init();
        if (!crl) {
                err = ERR_CURL_INIT;
                goto fail;
        }
        curl_easy_setopt(crl, CURLOPT_URL, url);
        free(url);
        url = 0;
        curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, curl_callback_mem_write);
        curl_easy_setopt(crl, CURLOPT_WRITEDATA, &buf_ret);
        list = curl_slist_append(list, auth);
        free(auth);
        auth = 0;
        curl_easy_setopt(crl, CURLOPT_HTTPHEADER, list);
        crlcode = curl_easy_perform(crl);
        curl_easy_cleanup(crl);
        curl_slist_free_all(list);
        if (crlcode != CURLE_OK)
                return curl_easy_strerror(crlcode);
        err = ttv_json_parse(svc, buf_ret.p);
        free(buf_ret.p);
        return err;
fail:
        free(url);
        free(auth);
        curl_easy_cleanup(crl);
        curl_slist_free_all(list);
        return err;
}

const char *
ttv_stream_play(const struct svc *svc, const char *c, const char *q)
{
        (void) svc;
        const char *ret;
        char *url;

        url = printma("https://twitch.tv/%s", c);
        if (!url)
                return ERR_MEM;
        ret = run_mpv_ytdl(url, q);
        free(url);
        return ret;
}

char *
ttv_stream_quality(const struct svc *svc, const char *c)
{
        (void) svc;
        char *ret;
        char *url;

        url = printma("https://twitch.tv/%s", c);
        if (!url)
                return 0;
        ret = quality_ytdl(url);
        free(url);
        return ret;
}

void
ttv_cleanup(struct svc *svc)
{
        struct ttv_s *ttv = svc->handle;

        local_free(&ttv->local);
        ttv_nonlocal_free(svc);
}

size_t
ttv_up_count(const struct svc *svc)
{
        const struct ttv_s *ttv = svc->handle;

        return ttv->up_cnt;
}

size_t
ttv_total_count(const struct svc *svc)
{
        const struct ttv_s *ttv = svc->handle;

        return ttv->local.offs_len;
}

char **
ttv_chans(const struct svc *svc)
{
        const struct ttv_s *ttv = svc->handle;

        return ttv->offs_name;
}

char **
ttv_status(const struct svc *svc)
{
        const struct ttv_s *ttv = svc->handle;

        return ttv->offs_status;
}

char **
ttv_game(const struct svc *svc)
{
        const struct ttv_s *ttv = svc->handle;

        return ttv->offs_game;
}
