/* Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
Copyright (c) 2019, Fortian Inc.
All rights reserved.

See the file LICENSE which should have accompanied this software. */

#ifndef __MWMG_H
#define __MWMG_H

#include <gtk/gtk.h>

#define DEFNSYS 20
#if 0
/* Default to 3 hours 5 minutes of samples */
#define DEFTLEN (3 * 3700)
#else
/* Default to 31 minutes of samples */
#define DEFTLEN (31 * 60)
#endif
#define NYMARKS 5
/* Calculate per-second averages every INTERVAL seconds */
#define INTERVAL 1
#define TSINTER 180

#define CONFFILE "mwmg.conf"
#define WINPOS ".mwmg.winprops"

#define M_LEFT (4 * cwidth + 1)
#define M_RIGHT (1 + cheight)
#define M_TOP (cheight / 2)
#define M_BOT (cheight)

enum e_wins {
#ifdef SHOW_STATISTICS
    W_MEM = 0, W_CPU,
#endif
    /* W_TIME, */
    W_NET, W_OTHER, W_USER
};

struct win {
    /* samples are oriented by: time X nsys */
    unsigned long *samps;
    char *name;
    int rm[2]; /* right margin */
    int sperpx;
    int ncfg;
    int dx, dy, cx, cy, cw, ch;
    unsigned int maxes[2];
    guint ctxid;
    GtkWindow *w;
    GtkDrawingArea *das[2];
    GdkPixmap *dbs[2];
    GdkGC *gc;
    GtkStatusbar *sb;
    GdkColor col[2];
};

#define S(x) SS(x)
#define SS(x) #x

#endif
