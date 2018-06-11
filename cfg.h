#ifndef CFG_H
#define CFG_H

struct cfg_s {
        char *x11_term;
        char *cfg_home;
        int refresh_timeout;
        int k_run;
        int k_quit;
        int k_down;
        int k_down_vi;
        int k_up;
        int k_up_vi;
        int k_left;
        int k_left_vi;
        int k_right;
        int k_right_vi;
        int k_home;
        int k_end;
        int k_update;
        int k_quality_change_up;
        int k_quality_change_down;
        int k_quality_fetch;
};

extern struct cfg_s cfg;

void cfg_homeset(void);
void cfg_parse(void);
void cfg_keyset(void);

#endif
