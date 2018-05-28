#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <ncurses.h>

#include "cfg.h"
#include "interface.h"
#include "quality.h"
#include "svc.h"
#include "svc_active.h"
#include "util.h"

#define PAD_WIDTH 256

struct {
        WINDOW *status;
        struct nc_svc *svc;
        size_t n_svc;
        size_t cur_src;
        size_t cur_quality;
        int w;
        int h;
} nc;

void
nc_start(void)
{
        initscr();
        cbreak();
        keypad(stdscr, 1);
        noecho();
        curs_set(1);
        clear();
        refresh();
        nc_populate();
}

void
nc_populate(void)
{
        size_t i;

        nc.n_svc = svc_arrsz();
        nc.svc = malloc(nc.n_svc * sizeof *nc.svc);
        for (i = 0; i < nc.n_svc; i++)
                nc.svc[i] = (struct nc_svc) {0};
}

void
nc_end(void)
{
        endwin();
}

void
nc_scroll_reset(struct nc_svc *svc)
{
        svc->cur = 0;
        svc->y_scroll = 0;
        svc->x_scroll = 0;
        move(0, 0);
        refresh();
}

static inline
bool
nc_term_undersize(struct nc_svc *svc)
{
        if (svc->n_entry > (size_t) nc.h - 1)
                return 1;
        return 0;
}

static inline
const char *
svc_perform(struct svc *svc, struct nc_svc *nc_svc)
{
        const char *ret;

        ret = svc->perform(svc);
        nc_svc->n_entry = svc->up_count(svc);
        nc_svc->cur = 0;
        nc_svc->y_scroll = 0;
        nc_svc->x_scroll = 0;
        return ret;
}

static inline
const char *
svc_stream_play(struct svc *svc, size_t i, const char *q)
{
        return svc->stream_play(svc, svc->chans(svc)[i], q);
}

static inline
void
nc_pad_refresh(struct nc_svc *svc)
{
        prefresh(svc->pad, svc->y_scroll, svc->x_scroll, 0, 0, nc.h - 2,
                        nc.w - 1);
}

static inline
void
nc_status_write(const char *fmt, ...)
{
        va_list ap;

        wclear(nc.status);
        va_start(ap, fmt);
        vw_printw(nc.status, fmt, ap);
        va_end(ap);
}

void
nc_scroll_reset_end(struct nc_svc *svc)
{
        if (!svc->n_entry)
                return;
        svc->cur = svc->n_entry - 1;
        svc->x_scroll = 0;
        if (!nc_term_undersize(svc))
                return;
        if (svc->n_entry - ((size_t) nc.h - 1) > INT_MAX)
                svc->y_scroll = INT_MAX;
        else if (nc_term_undersize(svc))
                svc->y_scroll = svc->n_entry - ((size_t) nc.h - 1);
        else
                svc->y_scroll = 0;
        move(svc->cur - svc->y_scroll, 0);
        refresh();
}

void
nc_scroll_vert(struct nc_svc *svc, int dir)
{
        int dir_abs;
        int i;

        if (!svc->n_entry)
                return;
        if (dir == INT_MIN)
                dir_abs = INT_MAX;
        else
                dir_abs = abs(dir);
        if (dir < 0) for (i = 0; i < dir_abs; i++) {
                if (!svc->cur) {
                        nc_scroll_reset_end(svc);
                        continue;
                }
                svc->cur--;
                if (nc_term_undersize(svc) && svc->y_scroll)
                        svc->y_scroll--;
        }
        else for (i = 0; i < dir_abs; i++) {
                if (svc->cur == svc->n_entry - 1) {
                        nc_scroll_reset(svc);
                        continue;
                }
                svc->cur++;
                if (nc_term_undersize(svc)
                        && (size_t) svc->y_scroll < svc->n_entry
                                - ((size_t) nc.h - 1))
                        svc->y_scroll++;
        }
}

void
nc_scroll_horiz(struct nc_svc *svc, int dir)
{
        int dir_abs;
        int max;

        if (!svc->n_entry)
                return;
        if (nc.w - 1 > PAD_WIDTH)
                return;
        max = PAD_WIDTH - (nc.w - 1);
        if (dir == INT_MIN)
                dir_abs = INT_MAX;
        else
                dir_abs = abs(dir);
        if (dir < 0) {
                if (dir_abs > svc->x_scroll)
                        svc->x_scroll = 0;
                else
                        svc->x_scroll += dir;
        }
        else {
                if (dir_abs > max - svc->x_scroll)
                        svc->x_scroll = max;
                else
                        svc->x_scroll += dir;
        }
}

#define NC_SIZE_MIN 2

void
nc_minsize(void)
{
        int y_max;
        timeout(-1);
inf:
        y_max = getmaxy(stdscr);
        if (y_max >= 2)
                return;
        clear();
        printw("terminal is %d rows tall, but at least %d are required",
                        y_max, NC_SIZE_MIN);
        refresh();
        while (getch() != KEY_RESIZE);
        goto inf;
}

void
nc_resize(struct nc_svc *svc)
{
        nc_scroll_reset(svc);
        nc_minsize();
        getmaxyx(stdscr, nc.h, nc.w);
        if (svc->pad) {
                delwin(svc->pad);
                delwin(nc.status);
                svc->pad = 0;
        }
        nc_pad_free(svc);
        nc_pad_alloc(svc);
        nc.status = newwin(1, nc.w, nc.h - 1, 0);
        if (!nc.status) {
                nc_end();
                HCF0(ERR_MEM);
                exit(1);
        }
        keypad(nc.status, 1);
}

void
nc_pad_alloc(struct nc_svc *svc)
{
        if (!svc->n_entry)
                return;
        svc->pad = newpad(svc->n_entry > INT_MAX ? INT_MAX : svc->n_entry,
                        PAD_WIDTH);
        if (!svc->pad) {
                nc_end();
                HCF0(ERR_MEM);
                exit(1);
        }
}

void
nc_pad_free(struct nc_svc *svc)
{
        if (!svc->pad)
                return;
        delwin(svc->pad);
        svc->pad = 0;
}

void
nc_pad_draw(struct nc_svc *svc, char **chan, char **game, char **extra,
                char **status)
{
        size_t i;

        if (!svc->pad)
                return;
        wclear(svc->pad);
        if (!svc->n_entry) {
                wattron(svc->pad, A_BLINK);
                mvwaddstr(svc->pad, 0, 0, "nothing here but us chickens");
                wattroff(svc->pad, A_BLINK);
                return;
        }
        for (i = 0; i < svc->n_entry; i++) {
                mvwaddstr(svc->pad, i, 0, "");
                if (chan)
                        waddstr(svc->pad, chan[i]);
                if (game) {
                        wattron(svc->pad, A_REVERSE);
                        waddch(svc->pad, '[');
                        waddstr(svc->pad, game[i]);
                        waddch(svc->pad, ']');
                        wattroff(svc->pad, A_REVERSE);
                }
                if (extra) {
                        waddch(svc->pad, '{');
                        waddstr(svc->pad, extra[i]);
                        waddch(svc->pad, '}');
                }
                if (status) {
                        wattron(svc->pad, A_BOLD);
                        waddstr(svc->pad, status[i]);
                        wattroff(svc->pad, A_BOLD);
                }
        }
}

void
nc_cleanup(void)
{
        size_t i;

        if (nc.status) {
                delwin(nc.status);
                nc.status = 0;
        }
        if (!nc.svc)
                return;
        for (i = 0; i < nc.n_svc; i++)
                nc_pad_free(nc.svc + i);
        free(nc.svc);
        nc.svc = 0;
        nc.n_svc = 0;
        nc.cur_src = 0;
        nc.w = 0;
        nc.h = 0;
}

void
nc_loop_main(void)
{
        const char *err;
        struct svc *svc;
        char timebuf[32];
        struct tm *time_last;
        time_t update_last;
        size_t cur_old;
        int x_scroll_old;
        int ch;

fetch:
        svc = svc_arr() + nc.cur_src;
        err = svc_perform(svc, nc.svc + nc.cur_src);
        update_last = time(0);
        time_last = localtime(&update_last);
        strftime(timebuf, sizeof timebuf, "%j:%H%M%S", time_last);
        timebuf[sizeof timebuf - 1] = 0;
resize:
        clear();
        nc_resize(nc.svc + nc.cur_src);
redraw:
        nc_pad_draw(nc.svc + nc.cur_src, svc->chans(svc), svc->game(svc), 0,
                        svc->status(svc));
        nc_pad_refresh(nc.svc + nc.cur_src);
redraw_status:
        nc_status_write("%s[%zu/%zu]:{%zu/%zu}(%s)%s%s", svc->name,
                        nc.svc[nc.cur_src].cur + 1, svc->up_count(svc),
                        nc.cur_quality + 1, quality_ytdl_arrsz(),
                        quality_ytdl_arr()[nc.cur_quality],
                        err ? err : "",
                        timebuf);
poll:
        wrefresh(nc.status);
        move(nc.svc[nc.cur_src].cur - nc.svc[nc.cur_src].y_scroll, 0);
        refresh();
        wtimeout(nc.status, cfg.refresh_timeout);
        ch = wgetch(nc.status);
        if (ch == cfg.k_run || ch == '\r' || ch == KEY_ENTER)
                goto run;
        else if (ch == cfg.k_down || ch == cfg.k_down_vi)
                goto down;
        else if (ch == cfg.k_up || ch == cfg.k_up_vi)
                goto up;
        else if (ch == cfg.k_left || ch == cfg.k_left_vi)
                goto left;
        else if (ch == cfg.k_right || ch == cfg.k_right_vi)
                goto right;
        else if (ch == cfg.k_home)
                goto home;
        else if (ch == cfg.k_end)
                goto end;
        else if (ch == cfg.k_update)
                goto update;
        else if (ch == cfg.k_quality_change_up)
                goto quality_up;
        else if (ch == cfg.k_quality_change_down)
                goto quality_down;
        else if (ch == cfg.k_quit)
                goto ret;
        else if (ch == KEY_RESIZE)
                goto resize;
        else if (ch == ERR)
                goto update;
        goto poll;
run:
        if (!nc.svc[nc.cur_src].n_entry)
                goto poll;
        nc_status_write("%s: now playing %s/%s", svc->name, svc->name,
                        svc->chans(svc)[nc.svc[nc.cur_src].cur]);
        wrefresh(nc.status);
        svc_stream_play(svc, nc.svc[nc.cur_src].cur,
                        quality_ytdl_arr()[nc.cur_quality]);
        wtimeout(nc.status, 3000);
        ch = wgetch(nc.status);
        wtimeout(nc.status, -1);
        goto redraw_status;
update:
        nc_status_write("%s: fetching data", svc->name);
        wrefresh(nc.status);
        nc_scroll_reset(nc.svc + nc.cur_src);
        goto fetch;
down:
        cur_old = nc.svc[nc.cur_src].cur;
        nc_scroll_vert(nc.svc + nc.cur_src, 1);
        if (nc_term_undersize(nc.svc + nc.cur_src))
                goto redraw;
        if (cur_old != nc.svc[nc.cur_src].cur)
                goto redraw_status;
        goto poll;
up:
        cur_old = nc.svc[nc.cur_src].cur;
        nc_scroll_vert(nc.svc + nc.cur_src, -1);
        if (nc_term_undersize(nc.svc + nc.cur_src))
                goto redraw;
        if (cur_old != nc.svc[nc.cur_src].cur)
                goto redraw_status;
        goto poll;
left:
        x_scroll_old = nc.svc[nc.cur_src].x_scroll;
        nc_scroll_horiz(nc.svc + nc.cur_src, -8);
        if (x_scroll_old != nc.svc[nc.cur_src].x_scroll)
                goto redraw;
        goto poll;
right:
        x_scroll_old = nc.svc[nc.cur_src].x_scroll;
        nc_scroll_horiz(nc.svc + nc.cur_src, 8);
        if (x_scroll_old != nc.svc[nc.cur_src].x_scroll)
                goto redraw;
        goto poll;
home:
        cur_old = nc.svc[nc.cur_src].cur;
        x_scroll_old = nc.svc[nc.cur_src].x_scroll;
        nc_scroll_reset(nc.svc + nc.cur_src);
        if (nc_term_undersize(nc.svc + nc.cur_src)
                || x_scroll_old != nc.svc[nc.cur_src].x_scroll)
                goto redraw;
        if (cur_old != nc.svc[nc.cur_src].cur)
                goto redraw_status;
        goto poll;
end:
        cur_old = nc.svc[nc.cur_src].cur;
        x_scroll_old = nc.svc[nc.cur_src].x_scroll;
        nc_scroll_reset_end(nc.svc + nc.cur_src);
        if (nc_term_undersize(nc.svc + nc.cur_src)
                || x_scroll_old != nc.svc[nc.cur_src].x_scroll)
                goto redraw;
        if (cur_old != nc.svc[nc.cur_src].cur)
                goto redraw_status;
        goto poll;
quality_up:
        if (!quality_ytdl_arrsz())
                goto poll;
        if (!nc.cur_quality)
                nc.cur_quality = quality_ytdl_arrsz() - 1;
        else
                nc.cur_quality--;
        goto redraw_status;
quality_down:
        if (!quality_ytdl_arrsz())
                goto poll;
        if (nc.cur_quality == quality_ytdl_arrsz() - 1)
                nc.cur_quality = 0;
        else
                nc.cur_quality++;
        goto redraw_status;
ret:
        return;
}
