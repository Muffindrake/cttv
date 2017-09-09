#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <bsd/string.h>

#include <curl/curl.h>
#include <jansson.h>
#include <ncurses.h>

#define ARRSZ(arr) (sizeof arr / sizeof *arr)

#ifndef VPLAYER
#       define VPLAYER "mpv"
#endif

#define TTVAPI "https://api.twitch.tv/kraken/streams?channel=%s\
&limit=100&stream_type=live&api_version=3\
&client_id=onsyu6idu0o41dl4ixkofx6pqq7ghn"
#define HIT_ANY_KEY "Hit the any key to return."

const char *help = ""
"Read the source file or README for help.\n";

#define DEFAULT_QUALITY_INDEX 4
const char *quality[] = {
        "best",
        "best[height <=? 1440]/best",
        "best[height <=? 1080]/best",
        "best[height <=? 720]/best",
        "best[height <=? 720][tbr <=? 2500]/best",
        "best[height <=? 480][tbr <=? 2250]/best",
        "best[height <=? 360][tbr <=? 1750]/best",
        "best[height <=? 480]/best",
        "best[height <=? 360]/best",
        "best[height <=? 160]/best",
        "worst",
        "audio",
        "1440p60/best",
        "1080p60/best",
        "720p60/best",
        "480p60/best",
        "360p60/best",
        "1440p30/best",
        "1080p30/best",
        "720p30/best",
        "480p30/best",
        "360p30/best",
        "best[tbr <=? 6000]/best",
        "best[tbr <=? 5000]/best",
        "best[tbr <=? 4000]/best",
        "best[tbr <=? 3500]/best",
        "best[tbr <=? 3250]/best",
        "best[tbr <=? 3000]/best",
        "best[tbr <=? 2500]/best",
        "best[tbr <=? 2000]/best",
        "best[tbr <=? 1500]/best",
        "best[tbr <=? 1000]/best",
        "best[tbr <=? 500]/best"
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
        const char *q;
        char *vpl;
        int h;
        int cur;
        int scry;
        unsigned int stat_only  :1;
};

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

void
update_url_str(const struct chan_ent *ent, char *s_buf, size_t sbsz,
                char *urlbuf, size_t ubsz)
{
        size_t i;
        size_t n_offs;

        for (s_buf[0] = 0, i = 0, n_offs = 0; i < ent->len - 1; i++) {
                n_offs += strlcat(s_buf + n_offs, ent->offset[i],
                                n_offs ? sbsz - (n_offs + 1) : sbsz);
                n_offs += strlcat(s_buf + n_offs, ",",
                                n_offs ? sbsz - (n_offs + 1) : sbsz);
        }

        strlcat(s_buf + n_offs, ent->offset[i],
                        n_offs ? sbsz - (n_offs + 1) : sbsz);
        snprintf(urlbuf, ubsz, TTVAPI, s_buf);
}

size_t
get_lines(const char *path, struct chan_ent *ent, char **s_buf, size_t *sbsz,
                char **urlbuf, size_t *ubsz)
{
        size_t i;
        size_t l;

        FILE *f = fopen(path, "r");
        if (!f)
                return 0;

        ent->len = 0;
        for (; EOF != fscanf(f, "%*[^\n]") && EOF != fscanf(f, "%*c");)
                ent->len++;
        if (!ent->len)
                goto err;

        *sbsz = (l = ftell(f)) + ent->len + 1;
        *s_buf = realloc(*s_buf, *sbsz);
        *ubsz = *sbsz + sizeof TTVAPI;
        *urlbuf = realloc(*urlbuf, *ubsz);

        ent->offset = realloc(ent->offset, ent->len * sizeof(void *) + l + 1);
        rewind(f);
        ent->data = (char *) (ent->offset + ent->len);

        l = fread(ent->data, 1, l, f);
        ent->data[l - 1] = 0;

        ent->offset[0] = ent->data;
        for (i = 1; i < ent->len; i++) {
                ent->offset[i] = strchr(ent->offset[i - 1], '\n') + 1;
                *(ent->offset[i] - 1) = 0;
        }

        update_url_str(ent, *s_buf, *sbsz, *urlbuf, *ubsz);

err:
        fclose(f);
        return ent->len;
}

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

void
run_live(const char *name, const char *q, const char *vpl,
                char *s_buf, const size_t sbsz)
{
        int ret;

        snprintf(s_buf, sbsz, "nohup youtube-dl 'https://twitch.tv/%s' "
                        "-f '%s' -o - | %s - "
                        ">/dev/null 2>/dev/null &",
                        name,
                        q,
                        vpl);
        ret = system(s_buf);
        if (ret) {
                endwin();
                fprintf(stderr, "Unable to launch shell: %s\n", s_buf);
                exit(1);
        }

        clear();
        printw("youtube-dl 'https://twitch.tv/%s' -f '%s' -o - | %s -",
                        name,
                        q,
                        vpl);
        refresh();
        usleep(1000000);
}

void
requests(struct status *stat, struct resp_ent *info, char *urlbuf)
{
        struct mem_data json_buf;
        size_t i;
        size_t n_offs;
        size_t g_offs;
        size_t t_offs;
        CURL *crl;
        CURLcode crlcode;
        json_t *root;
        json_t *streams;
        json_t *element;
        json_t *ch;
        json_t *rname;
        json_t *game;
        json_t *status;
        json_error_t error;

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

        if (JSON_ARRAY != json_typeof(streams))
                goto cleanup;

        info->len = json_array_size(streams);
        if (!info->len)
                goto cleanup;

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

        if (info->len > 1)
                quicksort(info, 0, info->len - 1);

cleanup:
        json_decref(root);
        curl_easy_cleanup(crl);
        free(json_buf.p);
}

void
draw_def(const struct status *stat, const struct resp_ent *info)
{
        int i;
        int y;

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

void
draw_stat(const struct status *stat, const struct resp_ent *info)
{
        int i;
        int y;

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

void
draw_all(struct status *stat, struct resp_ent *info)
{
        clear();
        if (!info->len) {
                addstr("No streams online or API dead.\n");
                attron(A_BOLD);
                addstr("Hit r to refresh. Hit R to reload file.");
                attroff(A_BOLD);
                refresh();
                return;
        }

        if (!stat->stat_only)
                draw_def(stat, info);
        else
                draw_stat(stat, info);

        refresh();
}

void
output_fmts(const char *name, char *s_buf, size_t sbsz)
{
        FILE *p;
        int ret;

        clear();
        attron(A_BOLD);
        addstr("Obtaining format information...\n\n");
        attroff(A_BOLD);
        snprintf(s_buf, sbsz,
                        "youtube-dl -F 'https://twitch.tv/%s' -o - 2>&1",
                        name);
        attron(A_STANDOUT);
        addstr(s_buf);
        attroff(A_STANDOUT);
        addch('\n');
        refresh();
        p = popen(s_buf, "re");
        if (!p) {
                endwin();
                fprintf(stderr, "Unable to open pipe for command: %s\n",
                                s_buf);
                exit(1);
        }

        while (fgets(s_buf, sbsz, p))
                addstr(s_buf);

        ret = pclose(p);
        if (ret)
                printw("youtube-dl returned non-zero exit status %d", ret);

        attron(A_BOLD);
        addstr("\nEnd of format list. " HIT_ANY_KEY "\n");
        attroff(A_BOLD);
        refresh();
        getch();
}

void
change_quality(struct status *stat, char *s_buf, size_t sbsz)
{
        unsigned long long x;
        size_t i;
        int ret;

        clear();
        attron(A_UNDERLINE);
        addstr("Current format: ");
        attroff(A_UNDERLINE);
        attron(A_STANDOUT);
        addstr(stat->q);
        attroff(A_STANDOUT);
        addstr("\n\n");
        for (i = 0; i < ARRSZ(quality); i++) {
                printw("%02zu:", i + 1);
                addstr(quality[i]);
                addch('\n');
        }

        attron(A_UNDERLINE);
        addstr("Select a format by entering an integer: ");
        attroff(A_UNDERLINE);
        refresh();
        echo();
        attron(A_BOLD);
        ret = getnstr(s_buf, sbsz > INT_MAX ? INT_MAX : sbsz);
        attroff(A_BOLD);
        noecho();
        addch('\n');
        if (ret != OK) {
                addstr("ncurses getnstr returned an error. " HIT_ANY_KEY);
                refresh();
                getch();
                return;
        }

        errno = 0;
        x = strtoull(s_buf, 0, 10);
        if (errno == EINVAL) {
                addstr("errno was set to EINVAL: invalid value. " HIT_ANY_KEY);
                refresh();
                getch();
                return;
        }
        if (errno == ERANGE || x > ARRSZ(quality)) {
                addstr("Conversion result out of range. " HIT_ANY_KEY);
                refresh();
                getch();
                return;
        }
        if (!x) {
                addstr("Invalid integer. " HIT_ANY_KEY);
                refresh();
                getch();
                return;
        }

        stat->q = quality[x - 1];
        attron(A_UNDERLINE);
        addstr("Format chosen: ");
        attroff(A_UNDERLINE);
        attron(A_REVERSE);
        addstr(stat->q);
        attroff(A_REVERSE);
        addstr("\n\n");
        addstr(HIT_ANY_KEY);
        refresh();
        getch();
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

        stat.vpl = getenv("VPLAYER");
        if (!stat.vpl)
                stat.vpl = VPLAYER;
        stat.q = quality[DEFAULT_QUALITY_INDEX];
        s_buf = malloc(sbsz = 1 << 12);

        if (argc == 1) {
                snprintf(s_buf, sbsz, "%s/.cttvrc", getenv("HOME"));
                if (!get_lines(s_buf, &ent, &s_buf, &sbsz, &urlbuf, &ubsz)) {
                        fputs("unable to parse lines in file\n", stderr);
                        return 1;
                }
        }
        else if (!strncmp(argv[1], "--help", 6)) {
                puts(help);
                return 0;
        }
        else if (!get_lines(argv[1], &ent, &s_buf, &sbsz, &urlbuf, &ubsz)) {
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
        requests(&stat, &info, urlbuf);
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
        case 'r':
                getmaxyx(stdscr, stat.h, ch);
                requests(&stat, &info, urlbuf);
                break;
        case 'R':
                if (!ent.offset)
                        break;
                free(ent.offset);
                ent.offset = 0;
                snprintf(s_buf, sbsz, "%s/.cttvrc", getenv("HOME"));
                if (!get_lines(argc == 1 ? s_buf : argv[1],
                                        &ent,
                                        &s_buf, &sbsz,
                                        &urlbuf, &ubsz)) {
                        endwin();
                        fputs("unable to parse lines in file\n", stderr);
                        return 1;
                }

                requests(&stat, &info, urlbuf);
                break;
        case ' ':
                stat.stat_only = !stat.stat_only;
                break;
        case '#':
                change_quality(&stat, s_buf, sbsz);
                break;
        case KEY_BACKSPACE:
                if (info.len)
                        output_fmts(info.name_offset[stat.cur], s_buf, sbsz);
                break;
        case KEY_ENTER:
        case '\r':
        case '\n':
                if (info.len)
                        run_live(info.name_offset[stat.cur], stat.q, stat.vpl,
                                        s_buf, sbsz);
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
