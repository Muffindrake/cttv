#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <glib.h>
#include <jansson.h>

#include "cfg.h"
#include "run.h"
#include "svc.h"
#include "svc_picarto.h"
#include "util.h"

struct ptv_s {
       char *data_name;
       char **offs_name;
       size_t up_cnt;
       char *api_key;
};

static
size_t
ptv_partition(struct svc *svc, size_t low, size_t high)
{
        struct ptv_s *ptv = svc->handle;
        char *swp;
        size_t i;
        int res;

        i = low - 1;
        high++;
inf:
        do {
                i++;
                res = strcmp(ptv->offs_name[i], ptv->offs_name[low]);
        } while (res < 0);
        do {
                high--;
                res = strcmp(ptv->offs_name[high], ptv->offs_name[low]);
        } while (res > 0);
        if (i >= high)
                return high;
        swp = ptv->offs_name[i];
        ptv->offs_name[i] = ptv->offs_name[high];
        ptv->offs_name[high] = swp;
        goto inf;
}

static
void
ptv_quicksort(struct svc *svc, size_t low, size_t high)
{
        size_t ret;

        if (low >= high)
                return;
        ret = ptv_partition(svc, low, high);
        ptv_quicksort(svc, low, ret);
        ptv_quicksort(svc, ret + 1, high);
}

static
void
ptv_nonlocal_free(struct svc *svc)
{
        struct ptv_s *ptv = svc->handle;

        free(ptv->data_name);
        ptv->data_name = 0;
        ptv->offs_name = 0;
        ptv->up_cnt = 0;
}

#define ERR_JSON_PARSE "unable to parse json"
#define ERR_JSON_ARRAY "unable to obtain array"
#define ERR_JSON_NOELEM "unable to obtain element from array"
#define ERR_JSON_NOSTATUS "unable to obtain status from element"
#define ERR_JSON_NOCHAN "unable to obtain channel from element"

static
const char *
ptv_json_parse(struct svc *svc, const char *json)
{
        struct ptv_s *ptv = svc->handle;
        const char *err;
        size_t i;
        size_t offs_name;
        json_t *root;
        json_t *elem;
        json_t *stat;
        json_t *chan;

        err = 0;
        if (ptv->data_name)
                ptv_nonlocal_free(svc);
        root = json_loads(json, 0, 0);
        if (!root)
                return ERR_JSON_PARSE;
        if (json_typeof(root) != JSON_ARRAY) {
                err = ERR_JSON_ARRAY;
                goto cleanup;
        }
        if (!(ptv->up_cnt = json_array_size(root)))
                goto cleanup;
        offs_name = 0;
        for (i = 0; i < ptv->up_cnt; i++) {
                elem = json_array_get(root, i);
                if (!elem) {
                        err = ERR_JSON_NOELEM;
                        goto cleanup;
                }
                stat = json_object_get(elem, "online");
                if (!stat) {
                        err = ERR_JSON_NOSTATUS;
                        goto cleanup;
                }
                if (json_typeof(stat) != JSON_TRUE)
                        break;
                chan = json_object_get(elem, "name");
                if (!chan || json_typeof(chan) != JSON_STRING) {
                        err = ERR_JSON_NOCHAN;
                        goto cleanup;
                }
                offs_name += json_string_length(chan) + 1;
        }
        if (!(ptv->up_cnt = i))
                goto cleanup;
        ptv->data_name = malloc(offs_name + offs_name % sizeof (void *)
                        + sizeof (void *) * 3 * ptv->up_cnt);
        ptv->offs_name = (char **)(ptv->data_name + offs_name
                        + offs_name % sizeof (void *));
        offs_name = 0;
        for (i = 0; i < ptv->up_cnt; i++) {
                elem = json_array_get(root, i);
                chan = json_object_get(elem, "name");
                ptv->offs_name[i] = ptv->data_name + offs_name;
                strcpy(ptv->offs_name[i], json_string_value(chan));
                murderize_single_quotes(ptv->offs_name[i]);
                offs_name += json_string_length(chan) + 1;
        }
        if (ptv->up_cnt > 1)
                ptv_quicksort(svc, 0, ptv->up_cnt - 1);
cleanup:
        json_decref(root);
        return err;
}

void
ptv_local_update(struct svc *svc)
{
        struct ptv_s *ptv;
        char *cfg_path;

        if (svc->handle)
                ptv_cleanup(svc);
        free(svc->handle);
        svc->handle = malloc(sizeof *ptv);
        if (!svc->handle)
                return;
        ptv = svc->handle;
        *ptv = (struct ptv_s) {0};
        cfg_path = printma("cfg_%s", svc->cfg_suf);
        if (!cfg_path)
                return;
        svc->api_key = ftos(cfg_path);
        free(cfg_path);
}

#define PTVAPI \
"https://api.picarto.tv/v1/user/following?priority_online=true"

const char *
ptv_perform(struct svc *svc)
{
        (void) svc;
        const char *err;
        char *auth;
        struct curl_cb_data buf_ret;
        struct curl_slist *list;
        CURL *crl;
        CURLcode crlcode;

        if (!svc->api_key)
                return "No Picarto API key in configuration";
        buf_ret = (struct curl_cb_data) {0};
        err = 0;
        list = 0;
        crl = curl_easy_init();
        if (!crl)
                return ERR_CURL_INIT;
        auth = printma("Authorization: Bearer %s", svc->api_key);
        if (!auth)
                return ERR_MEM;
        curl_easy_setopt(crl, CURLOPT_URL, PTVAPI);
        curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, curl_callback_mem_write);
        curl_easy_setopt(crl, CURLOPT_WRITEDATA, &buf_ret);
        list = curl_slist_append(list, auth);
        free(auth);
        list = curl_slist_append(list,
                        "Accept: application/json;charset=utf-8");
        curl_easy_setopt(crl, CURLOPT_HTTPHEADER, list);
        crlcode = curl_easy_perform(crl);
        curl_easy_cleanup(crl);
        curl_slist_free_all(list);
        if (crlcode != CURLE_OK)
                return curl_easy_strerror(crlcode);
        err = ptv_json_parse(svc, buf_ret.p);
        free(buf_ret.p);
        if (err)
                return err;
        return 0;
}

const char *
ptv_stream_play(const struct svc *svc, const char *c, const char *q)
{
        (void) svc;
        (void) q;
        const char *ret;
        char *url;

        url = printma("https://picarto.tv/%s", c);
        if (!url)
                return ERR_MEM;
        ret = run_mpv_streamlink(url, "best");
        free(url);
        return ret;
}

void
ptv_cleanup(struct svc *svc)
{
        struct ptv_s *ptv = svc->handle;

        ptv_nonlocal_free(svc);
        free(ptv->api_key);
}

size_t
ptv_up_count(const struct svc *svc)
{
        const struct ptv_s *ptv = svc->handle;

        return ptv->up_cnt;
}

size_t
ptv_total_count(const struct svc *svc)
{
        (void) svc;

        return 0;
}

char **
ptv_chans(const struct svc *svc)
{
        const struct ptv_s *ptv = svc->handle;

        return ptv->offs_name;
}
