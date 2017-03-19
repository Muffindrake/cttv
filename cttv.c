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
        "medium,source",
        "source",
        "high",
        "low",
        "mobile",
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
};

static size_t
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

static size_t
get_lines(char *path, struct chan_ent *ent)
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
        
        ent->data = malloc((l = ftell(f)) + 1);
        rewind(f);
        ent->offset = malloc(n * sizeof(void *));

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

static void
scroll_up(struct status *stat, size_t n_onl)
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

static void
scroll_down(struct status *stat, size_t n_onl)
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

static void
left_click(struct status *stat, size_t n_onl, MEVENT *mev)
{
        if ((size_t) mev->y > n_onl - 1)
                return;
        if (n_onl < (size_t) stat->h) {
                stat->cur = mev->y;
                return;
        }
        stat->cur = stat->scry + mev->y;
        if ((size_t) stat->cur > n_onl - 1)
                stat->cur = n_onl - 1;
        stat->scry = stat->cur - 1;
        if (stat->scry < 0)
                stat->scry = 0;
        if ((size_t) stat->scry > n_onl - (size_t) stat->h)
                stat->scry = n_onl - stat->h;
}

static void
run_live(char *data, const char *q, char *s_buf, size_t sbsz)
{
        snprintf(s_buf, sbsz, "nohup streamlink twitch.tv/%s %s "
                        ">/dev/null 2>/dev/null &", 
                        data, 
                        q);
        system(s_buf);
        snprintf(s_buf, sbsz, "nohup notify-send -u low -t 2000 "
                        "\"streamlink twitch.tv/%s %s\" "
                        ">/dev/null 2>/dev/null &",
                        data,
                        q);
        system(s_buf);
        usleep(500);
}

static void
to_clipboard(char *data, char *s_buf, size_t sbsz)
{
        snprintf(s_buf, sbsz, "echo -n \"%s\" | xsel -psb", data);
        system(s_buf);
        snprintf(s_buf, sbsz, "nohup notify-send -u low -t 2000 "
                        "\'\"%s\" to clipboard\'"
                        ">/dev/null 2>/dev/null &", 
                        data);
        system(s_buf);
        usleep(500);
}

static void
requests(struct status *stat, struct chan_ent *ent, struct resp_ent *info, 
                char *s_buf, size_t sbsz, char *urlbuf, size_t ubsz)
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

        stat->cur = 0;
        stat->scry = 0;
        memset(&json_buf, 0, sizeof(struct mem_data));

        if (info->name_data) {
                free(info->name_data);
                free(info->name_offset);
                free(info->game_data);
                free(info->game_offset);
                free(info->title_data);
                free(info->title_offset);
                memset(info, 0, sizeof(struct resp_ent));
        }

        if (!(crl = curl_easy_init())) {
                endwin();
                fprintf(stderr, "unable to initalize curl_easy\n");
                exit(1);
        }

        s_buf[0] = 0;
        for (i = 0, n_offs = 0; i < ent->len - 1; i++) {
                n_offs += strlcat(s_buf + n_offs, ent->offset[i], 
                                n_offs ? sbsz - (n_offs + 1) : sbsz); 
                n_offs += strlcat(s_buf + n_offs, ",", 
                                n_offs ? sbsz - (n_offs + 1) : sbsz);
        }
        strlcat(s_buf + n_offs, ent->offset[i], 
                        n_offs ? sbsz - (n_offs + 1) : sbsz);

        snprintf(urlbuf, ubsz, TTVAPI, s_buf);

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
                fprintf(stderr, "unable to get streams object\n");
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

        info->name_data = malloc(n_offs);
        info->game_data = malloc(g_offs);
        info->title_data = malloc(t_offs);
        info->name_offset = malloc(info->len * sizeof(void *));
        info->game_offset = malloc(info->len * sizeof(void *));
        info->title_offset = malloc(info->len * sizeof(void *));        

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

cleanup:
        json_decref(root);
        curl_easy_cleanup(crl);
        free(json_buf.p);
}

static void
draw_def(struct status *stat, struct resp_ent *info)
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

                mvprintw(y, 0, "%s ", info->name_offset[i]);
                attron(A_STANDOUT);
                printw("%s", info->game_offset[i]);
                attroff(A_STANDOUT);
                printw(" ");
                attron(A_BOLD);
                printw("%s", info->title_offset[i]);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);

                clrtobot();
        }
}

static void
draw_stat(struct status *stat, struct resp_ent *info)
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
                mvprintw(y, 0, "%s", info->title_offset[i]);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);
                
                clrtobot();
        }
}

static void
draw_all(struct status *stat, struct resp_ent *info)
{
        clear();

        if (!info->len) {
                mvprintw(0, 0, "no streams online or API dead\n"
                                "hit R to refresh");
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
        
        static char s_buf[4096];
        static char urlbuf[4096];
        static struct status stat;
        static struct chan_ent ent;
        static struct resp_ent info;
        static int ch;
        static size_t i;
        static MEVENT mevent;

        if (argc == 1) {
                snprintf(s_buf, sizeof s_buf, "%s/.cttvrc", getenv("HOME"));
                if (!(ent.len = get_lines(s_buf, &ent))) {
                        fprintf(stderr, "unable to parse lines in file\n");
                        return 1;
                }
        }
        else if (!strncmp(argv[1], "--help", 6)) {
                printf("%s", help);
                return 0;
        }
        else if (!(ent.len = get_lines(argv[1], &ent))) {
                fprintf(stderr, "unable to parse lines in file\n");
                return 1;
        }

        initscr();
        raw();
        cbreak();
        keypad(stdscr, true);
        noecho();
        curs_set(0);
        mousemask(BUTTON1_PRESSED, 0);
        
        curl_global_init(CURL_GLOBAL_DEFAULT);

        stat.stat_only = false;
        memset(&info, 0, sizeof(struct resp_ent));

        getmaxyx(stdscr, stat.h, ch);
        requests(&stat, &ent, &info, s_buf, sizeof s_buf, 
                        urlbuf, sizeof urlbuf);
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
                requests(&stat, &ent, &info, s_buf, sizeof s_buf, 
                                urlbuf, sizeof urlbuf);
                break;
        case ' ':
                stat.stat_only = !stat.stat_only;
                break;
        case KEY_ENTER:
        case '\r':
        case '\n':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[0], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'S':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[1], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'H':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[2], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'L':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[3], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'P':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[4], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'W':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[5], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'A':
                if (info.len)
                        run_live(info.name_offset[stat.cur], quality[6], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'C':
                if (info.len) {
                        snprintf(urlbuf, sizeof s_buf, "https://twitch.tv/%s", 
                                        info.name_offset[stat.cur]);
                        to_clipboard(urlbuf, s_buf, sizeof s_buf);
                }
                goto poll;
        case 'G':
                if (info.len)
                        to_clipboard(info.game_offset[stat.cur], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case 'T':
                if (info.len)
                        to_clipboard(info.title_offset[stat.cur], 
                                        s_buf, sizeof s_buf);
                goto poll;
        case KEY_RESIZE:
                stat.cur = 0;
                stat.scry = 0;
                getmaxyx(stdscr, stat.h, ch);
                break;
        case KEY_MOUSE:
                if(getmouse(&mevent) == OK && mevent.bstate & BUTTON1_PRESSED)
                        left_click(&stat, info.len, &mevent);
                break;
        default:
                goto poll;
        }

        draw_all(&stat, &info);
        goto poll;
end:
        if (ent.len) {
                free(ent.data);
                free(ent.offset);
        }
        if (info.len && info.name_data) {
                free(info.name_data);
                free(info.name_offset);
                free(info.game_data);
                free(info.game_offset);
                free(info.title_data);
                free(info.title_offset);
        }

        curl_global_cleanup();
        endwin();

        return 0;
}
