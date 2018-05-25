#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdint.h>

#include <ncurses.h>

struct nc_svc {
        WINDOW *pad;
        size_t n_entry;
        size_t cur;
        int y_scroll;
        int x_scroll;
};

void nc_start(void);
void nc_populate(void);
void nc_end(void);
void nc_scroll_reset(struct nc_svc *);
void nc_scroll_reset_end(struct nc_svc *);
void nc_scroll_vert(struct nc_svc *, int);
void nc_minsize(void);
void nc_resize(struct nc_svc *);
void nc_pad_alloc(struct nc_svc *);
void nc_pad_free(struct nc_svc *);
void nc_pad_draw(struct nc_svc *, char **, char **, char **, char **);
void nc_cleanup(void);
void nc_loop_main(void);

#endif
