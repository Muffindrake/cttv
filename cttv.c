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

struct entry_chan {
        char s[26];
};

struct entry_game {
        char s[64];
};

struct entry_title {
        char s[256];
};

struct status {
        int h;
        int cur;
        int scry;
        unsigned int stat_only  :1;
        
        size_t len;
        size_t n_onl;
        size_t nstreams;
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
get_lines(char *path, struct entry_chan **ent, char *s_buf, size_t sbsz)
{
        size_t n;
        size_t l;
        size_t i;

        FILE *f = fopen(path, "r");
        if (!f)
                return 0;

        for (n = 0; EOF != fscanf(f, "%*[^\n]") && EOF != fscanf(f, "%*c");)
                n++;
        rewind(f);
        if (n > 100)
                n = 100;
        if (!n)
                goto err;
        
        *ent = malloc(n * sizeof(struct entry_chan));
        for (i = 0; i < n; i++) {
                if (!fgets(s_buf, sbsz, f)) {
                        n = 0; 
                        goto err;
                }
                l = strlen(s_buf);
                if (l > 0)
                        s_buf[l - 1] = 0;
                
                strncpy((*ent)[i].s, s_buf, sizeof(struct entry_chan) - 1);
                (*ent)[i].s[sizeof(struct entry_chan) - 1] = 0;
        }
        
err:
        fclose(f);
        return n;
}

static void
scroll_up(struct status *stat)
{
        if (!stat->n_onl)
                return;
        if (!stat->cur) {
                stat->cur = stat->n_onl - 1;
                if (stat->n_onl > (size_t) stat->h)
                        stat->scry = stat->n_onl - stat->h;
                return;
        }

        stat->cur--;
        if (stat->n_onl > (size_t) stat->h && stat->scry)
                stat->scry--;
}

static void
scroll_down(struct status *stat)
{
        if (!stat->n_onl)
                return;
        if ((size_t) stat->cur == stat->n_onl - 1) {
                stat->cur = 0;
                if (stat->n_onl > (size_t) stat->h)
                        stat->scry = 0;
                return;
        }

        stat->cur++;
        if (stat->n_onl > (size_t) stat->h 
                        && (size_t) stat->scry < stat->n_onl - (size_t) stat->h)
                stat->scry++;
}

static void
left_click(struct status *stat, MEVENT *mev)
{
        if (!stat->n_onl)
                return;
        if ((size_t) mev->y > stat->n_onl - 1)
                return;
        if (stat->n_onl < (size_t) stat->h) {
                stat->cur = mev->y;
                return;
        }
        stat->cur = stat->scry + mev->y;
        if ((size_t) stat->cur > stat->n_onl - 1)
                stat->cur = stat->n_onl - 1;
        stat->scry = stat->cur - 1;
        if (stat->scry < 0)
                stat->scry = 0;
        if ((size_t) stat->scry > stat->n_onl - (size_t) stat->h)
                stat->scry = stat->n_onl - stat->h;
}

static void
run_live(struct status *stat, struct entry_chan *name, const char *q, 
                char *s_buf, size_t sbsz)
{
        if (!stat->n_onl)
                return;

        snprintf(s_buf, sbsz,
                        "streamlink twitch.tv/%s %s "
                        "> /dev/null 2> /dev/null &",
                        name[stat->cur].s, q);
        system(s_buf);
        usleep(1000);
}

static void
to_clipboard(char *data, char *s_buf, size_t sbsz)
{
        snprintf(s_buf, sbsz, "echo -n \"%s\" | xsel -psb", 
                        data);
        system(s_buf);
        usleep(1000);
}

static void
requests(struct status *stat, struct entry_chan **ent,
                struct entry_chan **name, struct entry_game **gam, 
                struct entry_title **title, char *s_buf, size_t sbsz, 
                char *urlbuf, size_t ubsz)
{       
        static struct mem_data json_buf;
        static size_t i;
        static size_t offset;
        static CURL *crl;
        static CURLcode crlcode;
        static json_t *root;
        static json_t *streams;
        static json_t *element;
        static json_t *channel;
        static json_t *rname;
        static json_t *game;
        static json_t *status;
        static json_error_t error;

        stat->cur = 0;
        stat->scry = 0;
        json_buf.p = 0;
        json_buf.len = 0;

        if (*name) {
                free(*name);
                free(*gam);
                free(*title);
        }

        if (!(crl = curl_easy_init())) {
                endwin();
                fprintf(stderr, "unable to initalize curl_easy\n");
                exit(1);
        }

        s_buf[0] = 0;
        for (i = 0, offset = 0; i < stat->nstreams; i++) {
                offset += strlcat(s_buf + offset, (*ent)[i].s, 
                                offset ? sbsz - (offset + 1) : sbsz); 
                offset += strlcat(s_buf + offset, ",", 
                                offset ? sbsz - (offset + 1) : sbsz);
        }
        s_buf[sbsz - 1] = 0;

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
                stat->n_onl = 0;
                *name = 0;
                *gam = 0;
                *title = 0;
                goto cleanup;
        }

        stat->n_onl = json_array_size(streams);
        *name = malloc(stat->n_onl * sizeof(struct entry_chan));
        *gam = malloc(stat->n_onl * sizeof(struct entry_game));
        *title = malloc(stat->n_onl * sizeof(struct entry_title));
        for (i = 0; i < stat->n_onl; i++) {
                element = json_array_get(streams, i);
                channel = json_object_get(element, "channel");

                rname   = json_object_get(channel, "name");
                game    = json_object_get(channel, "game");
                status  = json_object_get(channel, "status");
                
                strncpy((*name)[i].s, json_string_value(rname),
                                sizeof(struct entry_chan) - 1);
                (*name)[i].s[sizeof(struct entry_chan) - 1] = 0;

                strncpy((*gam)[i].s, json_string_value(game), 
                                sizeof(struct entry_game) - 1);
                (*gam)[i].s[sizeof(struct entry_game) - 1] = 0;

                strncpy((*title)[i].s, json_string_value(status), 
                                sizeof(struct entry_title) - 1);
                (*title)[i].s[sizeof(struct entry_title) - 1] = 0;
        }

cleanup:
        json_decref(root);
        curl_easy_cleanup(crl);
        free(json_buf.p);
}

static void
draw_def(struct status *stat, struct entry_chan *name, struct entry_game *gam,
                struct entry_title *title)
{
        static int i;
        static int y;

        i = 0;
        y = -stat->scry;
        for (; (size_t) i < stat->n_onl && y < stat->h; 
                        i++, y = i - stat->scry) {
                if (y < 0)
                        continue;

                if (stat->cur == i)
                        attron(A_UNDERLINE);

                mvprintw(y, 0, "%s ", name[i].s);
                attron(A_STANDOUT);
                printw("%s", gam[i].s);
                attroff(A_STANDOUT);
                printw(" ");
                attron(A_BOLD);
                printw("%s", title[i].s);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);

                clrtobot();
        }
}

static void
draw_stat(struct status *stat, struct entry_title *title)
{
        static int i;
        static int y;

        i = 0;
        y = -stat->scry;
        for (; (size_t) i < stat->n_onl && y < stat->h
                        ; i++, y = i - stat->scry) {
                if (y < 0)
                        continue;

                if (stat->cur == i)
                        attron(A_UNDERLINE);

                attron(A_BOLD);
                mvprintw(y, 0, "%s", title[i].s);
                attroff(A_BOLD);

                if (stat->cur == i)
                        attroff(A_UNDERLINE);
                
                clrtobot();
        }
}

static void
draw_all(struct status *stat, struct entry_chan *name,
                struct entry_game *gam, struct entry_title *title)
{
        clear();

        if (!stat->n_onl) {
                mvprintw(0, 0, "no streams online or API dead\n"
                                "hit R to refresh");
                refresh();
                return;
        }

        if (!stat->stat_only)
                draw_def(stat, name, gam, title);
        else
                draw_stat(stat, title);

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
        static struct entry_chan *ent;
        static struct entry_chan *name;
        static struct entry_game *gam;
        static struct entry_title *title;
        static int ch;
        static size_t i;
        static MEVENT mevent;

        if (argc == 1) {
                snprintf(s_buf, sizeof s_buf, "%s/.cttvrc", getenv("HOME"));
                if (!(stat.nstreams = get_lines(s_buf, &ent, s_buf, 
                                                sizeof s_buf))) {
                        fprintf(stderr, "unable to parse lines in file\n");
                        return 1;
                }
        }
        else if (!strncmp(argv[1], "--help", 6)) {
                printf("%s", help);
                return 0;
        }
        else {
                if (!(stat.nstreams = get_lines(argv[1], &ent, s_buf, 
                                                sizeof s_buf))) {
                        fprintf(stderr, "unable to parse lines in file\n");
                        return 1;
                }
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
        name = 0;
        gam = 0;
        title = 0;

        getmaxyx(stdscr, stat.h, ch);
        requests(&stat, &ent, &name, &gam, &title, s_buf, sizeof s_buf, 
                        urlbuf, sizeof urlbuf);
        draw_all(&stat, name, gam, title);
poll:
        ch = getch();
        switch (ch) {
        case 'Q':
                goto end;
        case KEY_DOWN:
        case 'j':
                scroll_down(&stat);
                break;
        case KEY_UP:
        case 'k':
                scroll_up(&stat);
                break;
        case KEY_HOME:
                stat.cur = 0;
                stat.scry = 0;
                break;
        case KEY_END:
                stat.cur = stat.n_onl - 1;
                if (stat.n_onl > (size_t) stat.h)
                        stat.scry  = stat.n_onl - (size_t) stat.h;
                break;
        case 'K':
                for (i = 0; i < 5; i++)
                        scroll_up(&stat);
                break;
        case 'J':
                for (i = 0; i < 5; i++)
                        scroll_down(&stat);
                break;
        case 'R':
                getmaxyx(stdscr, stat.h, ch);
                requests(&stat, &ent, &name, &gam, &title, s_buf, sizeof s_buf, 
                                urlbuf, sizeof urlbuf);
                break;
        case ' ':
                stat.stat_only = !stat.stat_only;
                break;
        case KEY_ENTER:
        case '\r':
        case '\n':
                run_live(&stat, name, quality[0], s_buf, sizeof s_buf);
                goto poll;
        case 'S':
                run_live(&stat, name, quality[1], s_buf, sizeof s_buf);
                goto poll;
        case 'H':
                run_live(&stat, name, quality[2], s_buf, sizeof s_buf);
                goto poll;
        case 'L':
                run_live(&stat, name, quality[3], s_buf, sizeof s_buf);
                goto poll;
        case 'P':
                run_live(&stat, name, quality[4], s_buf, sizeof s_buf);
                goto poll;
        case 'W':
                run_live(&stat, name, quality[5], s_buf, sizeof s_buf);
                goto poll;
        case 'A':
                run_live(&stat, name, quality[6], s_buf, sizeof s_buf);
                goto poll;
        case 'C':
                if (!stat.n_onl)
                        goto poll;
                snprintf(urlbuf, sizeof s_buf, "https://twitch.tv/%s", 
                                name[stat.cur].s);
                to_clipboard(urlbuf, s_buf, sizeof s_buf);
                goto poll;
        case 'G':
                if (!stat.n_onl)
                        goto poll;
                to_clipboard(gam[stat.cur].s, s_buf, sizeof s_buf);
                goto poll;
        case 'T':
                if (!stat.n_onl)
                        goto poll;
                to_clipboard(title[stat.cur].s, s_buf, sizeof s_buf);
                goto poll;
        case KEY_RESIZE:
                stat.cur = 0;
                stat.scry = 0;
                getmaxyx(stdscr, stat.h, ch);
                break;
        case KEY_MOUSE:
                if(!(getmouse(&mevent) == OK))
                        goto poll;
                if(mevent.bstate & BUTTON1_PRESSED) {
                        left_click(&stat, &mevent);
                        break;
                }
                else {
                        goto poll;
                }
        default:
                goto poll;
        }

        draw_all(&stat, name, gam, title);
        goto poll;
end:
        free(ent);
        free(name);
        free(gam);
        free(title);

        curl_global_cleanup();
        endwin();

        return 0;
}
