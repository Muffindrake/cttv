#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <bsd/string.h>

#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>

#define TTVAPI "https://api.twitch.tv/kraken/streams?channel=%s\
&limit=100&stream_type=live&api_version=3\
&client_id=onsyu6idu0o41dl4ixkofx6pqq7ghn"

const char *help = ""
"Read the source file or https://github.com/muffindrake/cttv for help.\n";

const char *quality[] = {
        "medium,source,480p,best",
        "source,best",
        "high,720p60,720p,best",
        "low,360p,best",
        "mobile,144p,best",
        "worst",
        "audio"
};

struct mem_data {
        char *p;
        size_t len;
};

struct chan_ent {
        char **offset;
        size_t len;
        char *data;
};

struct resp_ent {
        char  *name_data;
        char **name_offset;
        char  *game_data;
        char **game_offset;
        char  *title_data;
        char **title_offset;
        size_t len;
};

struct status {
        int h;
        int cur;
        int scry;
        unsigned int stat_only  :1;
        unsigned int str_concat :1;
};

static
size_t
partition(struct resp_ent *rent, const size_t lo, size_t hi)
{
        static char *swp;
        static size_t i;
        static int res;

        i = lo - 1;
        hi++;

        while (1) {
                do {
                        i++;
                        res = strcmp(rent->name_offset[i], rent->name_offset[lo]);
                }
                while(res < 0);

                do {
                        hi--;
                        res = strcmp(rent->name_offset[hi], rent->name_offset[lo]);
                }
                while(res > 0);

                if (i >= hi)
                        return hi;

                swp = rent->name_offset[i];
                rent->name_offset[i] = rent->name_offset[hi];
                rent->name_offset[hi] = swp;
                swp = rent->game_offset[i];
                rent->game_offset[i] = rent->game_offset[hi];
                rent->game_offset[hi] = swp;
                swp = rent->title_offset[i];
                rent->title_offset[i] = rent->title_offset[hi];
                rent->title_offset[hi] = swp;
        }
}

static
void
quicksort(struct resp_ent *rent, const size_t lo, const size_t hi)
{
        static size_t p;

        if (lo < hi) {
                p = partition(rent, lo, hi);
                quicksort(rent, lo, p);
                quicksort(rent, p + 1, hi);
        }
}

static
size_t
mem_write_callback(void *cont, size_t size, size_t nmemb, void *p)
{
        size_t rsize = size * nmemb;
        struct mem_data *mem = p;

        mem->p = realloc(mem->p, mem->len + rsize + 1);
        if (!mem->p)
                return 0;

        memcpy(&(mem->p[mem->len]), cont, rsize);
        mem->len += rsize;
        mem->p[mem->len] = 0;

        return rsize;
}

static
size_t
get_lines(const char *path, struct chan_ent *ent, char **s_buf, size_t *sbsz,
                char **urlbuf, size_t *ubsz)
{
        size_t n;
        size_t i;
        size_t l;

        FILE *f = fopen(path, "r");
        if (!f)
                return 0;

        for (n = 0; EOF != fscanf(f, "%*[^\n]") && EOF != fscanf(f, "%*c");)
                n++;
        if (!n)
                goto err;

        *sbsz = (l = ftell(f)) + n + 1;
        *s_buf = realloc(*s_buf, *sbsz);
        *ubsz = *sbsz + sizeof TTVAPI;
        *urlbuf = realloc(*urlbuf, *ubsz);

        ent->offset = realloc(ent->offset, n * sizeof(void *) + l + 1);
        rewind(f);
        ent->data = (char *) &ent->offset[n];

        l = fread(ent->data, 1, l, f);
        ent->data[l - 1] = 0;

        ent->offset[0] = ent->data;
        for (i = 1; i < n; i++) {
                ent->offset[i] = strchr(ent->offset[i - 1], '\n') + 1;
                *(ent->offset[i] - 1) = 0;
        }

err:
        fclose(f);
        return n;
}

static
void
scroll_up(struct status *stat, const size_t n_onl)
{
        if (!stat->cur) {
                stat->cur = n_onl - 1;
                if (n_onl > (size_t) stat->h)
                        stat->scry = n_onl - stat->h;
                return;
        }

        stat->cur--;
        if (n_onl > (size_t) stat->h && stat->scry)
                stat->scry--;
}

static
void
scroll_down(struct status *stat, const size_t n_onl)
{
        if ((size_t) stat->cur == n_onl - 1) {
                stat->cur = 0;
                if (n_onl > (size_t) stat->h)
                        stat->scry = 0;
                return;
        }

        stat->cur++;
        if (n_onl > (size_t) stat->h
                        && (size_t) stat->scry < n_onl - (size_t) stat->h)
                stat->scry++;
}

static
void
run_live(const char *data, const int c, char *s_buf, const size_t sbsz)
{
        static unsigned char q;
        
        switch (c) {
        case KEY_ENTER:
        case '\n':
        case '\r':
                q = 0;
                break;
        case 'S':
                q = 1;
                break;
        case 'H':
                q = 2;
                break;
        case 'L':
                q = 3;
                break;
        case 'M':
                q = 4;
                break;
        case 'W':
                q = 5;
                break;
        case 'A':
                q = 6;
                break;
        }

        snprintf(s_buf, sbsz, "nohup streamlink \'twitch.tv/%s\' %s "
                        ">/dev/null 2>/dev/null &",
                        data,
                        quality[q]);
        system(s_buf);
        clear();
        mvprintw(0, 0, "streamlink twitch.tv/%s %s", data, quality[q]);
        refresh();
        usleep(1000000);
}

static
void
requests(struct status *stat, struct chan_ent *ent, struct resp_ent *info, 
                char *s_buf, const size_t sbsz, char *urlbuf, const size_t ubsz)
{
        static struct mem_data json_buf;
        static size_t i;

        static CURL *crl;
        static CURLcode crlcode;
        static json_t *root;
        static json_t *streams;
        static json_t *element;
        static json_t *ch;
        static json_t *rname;
        static json_t *game;
        static json_t *status;
        static json_error_t error;

        static size_t n_offs;
        static size_t g_offs;
        static size_t t_offs;

        clear();
        mvaddstr(0, 0, "Obtaining information from API...");
        refresh();

        stat->cur = 0;
        stat->scry = 0;
        memset(&json_buf, 0, sizeof json_buf);

        if (info->name_data) {
                free(info->name_data);
                memset(info, 0, sizeof *info);
        }

        if (!(crl = curl_easy_init())) {
                endwin();
                fputs("unable to initalize curl_easy\n", stderr);
                exit(1);
        }

        if (!stat->str_concat) {
                for (s_buf[0] = 0, i = 0, n_offs = 0; i < ent->len - 1; i++) {
                        n_offs += strlcat(s_buf + n_offs, ent->offset[i],
                                        n_offs ? sbsz - (n_offs + 1) : sbsz);
                        n_offs += strlcat(s_buf + n_offs, ",",
                                        n_offs ? sbsz - (n_offs + 1) : sbsz);
                }

                strlcat(s_buf + n_offs, ent->offset[i],
                                n_offs ? sbsz - (n_offs + 1) : sbsz);
                snprintf(urlbuf, ubsz, TTVAPI, s_buf);
                stat->str_concat = 1;
        }

        curl_easy_setopt(crl, CURLOPT_URL, urlbuf);
        curl_easy_setopt(crl, CURLOPT_WRITEFUNCTION, mem_write_callback);
        curl_easy_setopt(crl, CURLOPT_WRITEDATA, &json_buf);
        crlcode = curl_easy_perform(crl);
        if (CURLE_OK != crlcode) {
                endwin();
                fprintf(stderr, "unable to perform GET request: %s\n",
                                curl_easy_strerror(crlcode));
                exit(1);
        }

        root = json_loads(json_buf.p, 0, &error);
        if (!root) {
                endwin();
                fprintf(stderr, "unable to decode JSON: line %d: %s\n",
                                error.line, error.text);
                exit(1);
        }

        streams = json_object_get(root, "streams");
        if (!streams) {
                endwin();
                fputs("unable to get streams object\n", stderr);
                exit(1);
        }

        if (JSON_ARRAY != json_typeof(streams)) {
                goto cleanup;
        }

        info->len = json_array_size(streams);

        for (i = 0, n_offs = 0, g_offs = 0, t_offs = 0
                        ; i < info->len
                        ; i++) {
                element = json_array_get(streams, i);
                ch = json_object_get(element, "channel");

                n_offs += json_string_length(json_object_get(ch, "name")) + 1;
                g_offs += json_string_length(json_object_get(ch, "game")) + 1;
                t_offs += json_string_length(json_object_get(ch, "status")) + 1;
        }
        
        info->name_data = malloc(n_offs + g_offs + t_offs + 
                        sizeof(void *) * 3 * info->len);
        info->game_data = info->name_data + n_offs;
        info->title_data = info->game_data + g_offs;
        info->name_offset = (char **)(info->title_data + t_offs);
        info->game_offset = info->name_offset + info->len;
        info->title_offset = info->game_offset + info->len;

        for (i = 0, n_offs = 0, g_offs = 0, t_offs = 0
                        ; i < info->len
                        ; i++) {
                element = json_array_get(streams, i);
                ch = json_object_get(element, "channel");

                rname = json_object_get(ch, "name");
                strcpy(info->name_data + n_offs, json_string_value(rname));
                info->name_offset[i] = info->name_data + n_offs;
                n_offs += json_string_length(rname) + 1;

                game = json_object_get(ch, "game");
                strcpy(info->game_data + g_offs, json_string_value(game));
                info->game_offset[i] = info->game_data + g_offs;
                g_offs += json_string_length(game) + 1;

                status = json_object_get(ch, "status");
                strcpy(info->title_data + t_offs, json_string_value(status));
                info->title_offset[i] = info->title_data + t_offs;
                t_offs += json_string_length(status) + 1;
        }

        quicksort(info, 0, info->len - 1);

cleanup:
        json_decref(root);
        curl_easy_cleanup(crl);
        free(json_buf.p);
}

static
void
draw_def(const struct status *stat, const struct resp_ent *info)
{
        static int i;
        static int y;

        i = 0;
        y = -stat->scry;
        for (; (size_t) i < info->len && y < stat->h
                        ; i++, y = i - stat->scry) {
                if (y < 0)
                        continue;

                if (stat->cur == i)
                        attron(A_UNDERLINE);

                mvaddstr(y, 0, info->name_offset[i]);
                addstr(" ");
                attron(A_STANDOUT);
                addstr(info->game_offset[i]);
                attroff(A_STANDOUT);
                addstr(" ");
                attron(A_BOLD);
                addstr(info->title_offset[i]);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);

                clrtobot();
        }
}

static
void
draw_stat(const struct status *stat, const struct resp_ent *info)
{
        static int i;
        static int y;

        i = 0;
        y = -stat->scry;
        for (; (size_t) i < info->len && y < stat->h
                        ; i++, y = i - stat->scry) {
                if (y < 0)
                        continue;

                if (stat->cur == i)
                        attron(A_UNDERLINE);

                attron(A_BOLD);
                mvaddstr(y, 0, info->title_offset[i]);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);

                clrtobot();
        }
}

static
void
draw_all(struct status *stat, struct resp_ent *info)
{
        clear();

        if (!info->len) {
                mvaddstr(0, 0, "No streams online or API dead.\n"
                                "Hit R to refresh.");
                refresh();
                return;
        }

        if (!stat->stat_only)
                draw_def(stat, info);
        else
                draw_stat(stat, info);

        refresh();
}

int
main(int argc, char **argv)
{
        setlocale(LC_ALL, "");
        if (argc > 2) {
                fprintf(stderr, "Requires 1 argument, got %d\n", argc - 1);
                return 1;
        }

        static char *s_buf;
        static char *urlbuf;
        static struct status stat;
        static struct chan_ent ent;
        static struct resp_ent info;
        static int ch;
        static size_t i;
        static size_t sbsz;
        static size_t ubsz;

        s_buf = malloc(sbsz = 1024);

        if (argc == 1) {
                snprintf(s_buf, sbsz, "%s/.cttvrc", getenv("HOME"));
                if (!(ent.len = get_lines(s_buf, &ent, &s_buf, &sbsz,
                                                &urlbuf, &ubsz))) {
                        fputs("unable to parse lines in file\n", stderr);
                        return 1;
                }
        }
        else if (!strncmp(argv[1], "--help", 6)) {
                puts(help);
                return 0;
        }
        else if (!(ent.len = get_lines(argv[1], &ent, &s_buf, &sbsz,
                                        &urlbuf, &ubsz))) {
                fputs("unable to parse lines in file\n", stderr);
                return 1;
        }

        initscr();
        raw();
        cbreak();
        keypad(stdscr, true);
        noecho();
        curs_set(0);

        curl_global_init(CURL_GLOBAL_DEFAULT);

        getmaxyx(stdscr, stat.h, ch);
        requests(&stat, &ent, &info, s_buf, sbsz, urlbuf, ubsz);
        draw_all(&stat, &info);
poll:
        ch = getch();
        switch (ch) {
        case 'Q':
                goto end;
        case KEY_DOWN:
        case 'j':
                if (info.len)
                        scroll_down(&stat, info.len);
                break;
        case KEY_UP:
        case 'k':
                if (info.len)
                        scroll_up(&stat, info.len);
                break;
        case KEY_HOME:
                stat.cur = 0;
                stat.scry = 0;
                break;
        case KEY_END:
                stat.cur = info.len - 1;
                if (info.len > (size_t) stat.h)
                        stat.scry  = info.len - (size_t) stat.h;
                break;
        case 'K':
                if (info.len)
                        for (i = 0; i < 5; i++)
                                scroll_up(&stat, info.len);
                break;
        case 'J':
                if (info.len)
                        for (i = 0; i < 5; i++)
                                scroll_down(&stat, info.len);
                break;
        case 'R':
                getmaxyx(stdscr, stat.h, ch);
                requests(&stat, &ent, &info, s_buf, sbsz, urlbuf, ubsz);
                break;
        case ' ':
                stat.stat_only = !stat.stat_only;
                break;
        case KEY_ENTER:
        case '\r':
        case '\n':
        case 'S':
        case 'H':
        case 'L':
        case 'P':
        case 'W':
        case 'A':
                if (info.len)
                        run_live(info.name_offset[stat.cur], ch, s_buf, sbsz);
                break;
        case KEY_RESIZE:
                stat.cur = 0;
                stat.scry = 0;
                getmaxyx(stdscr, stat.h, ch);
                break;
        default:
                goto poll;
        }

        draw_all(&stat, &info);
        goto poll;
end:
        free(s_buf);
        free(urlbuf);
        if (ent.len)
                free(ent.offset);
        if (info.len && info.name_data)
                free(info.name_data);

        curl_global_cleanup();
        endwin();

        return 0;
}
