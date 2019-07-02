/* Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
Copyright (c) 2019, Fortian Inc.
All rights reserved.

See the file LICENSE which should have accompanied this software. */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <ctype.h>
#if defined(__unix__)
#include <sys/mman.h>
#elif defined(WINNT)
/* Be careful, it's not clear if Windows still works. */
#include <windows.h>
#include <tchar.h>
#else
#error "I don't know what platform this is."
#endif
#include <stdlib.h>
#include <math.h>
#ifdef __unix__
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __unix__
#include <fcntl.h>
#endif
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <errno.h>
/* #include "malloc.h" */
#include <sys/types.h>
#ifdef __unix__
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include "mwmg.h"

#if defined(DEBUG) && (__SIZEOF_LONG__ == __SIZEOF_WCHAR_T__)
#include <wchar.h>
#endif

#define BAR_MIN 3
#define BAR_PAD 1

#define FONT "fixed"
#define MINTHIGH 200
#define MINBHIGH 125
#define MINWIDTH ((BAR_PAD + BAR_MIN) * nsys + 2)

#define COLORSIZE 22

#define ALLOC_WIDTH(x) (x)->widget.allocation.width
#define ALLOC_HEIGHT(x) (x)->widget.allocation.height
#define WHITE_GC(x) (x)->widget.style->white_gc
#define BLACK_GC(x) (x)->widget.style->black_gc
#define FG_GC(x) (x)->widget.style->fg_gc

#if 0
#define REMOTE "/tmp/remotecontrol.mwmg"
#endif

struct win *wins = NULL;
unsigned long curtime = 0;
int nwins = 0;
int nsys = DEFNSYS;
int showwins = 0;

unsigned char gauge = 0;
time_t interval = INTERVAL;

#if 0
int encaptgt = -1;
#endif

enum legends {
    E_BITS,
    E_REQS,
    E_LAST
} curleg = E_BITS;

GdkPixmap *xpm = NULL;
#ifdef SHOW_STATISTICS
GdkPixmap *bytes = NULL;
GdkPixmap *percent = NULL;
#endif
/* GdkPixmap *usec = NULL; */
GtkSpinButton *wallsec = NULL;
GtkSpinButton *simsec = NULL;
GtkSpinButton *cursec = NULL;
GtkObject *adj = NULL;
GtkToggleButton *pausetest = NULL;

time_t start;
struct tm *whens;
int cwidth, cheight;

GdkColor gray;
GdkFont *font;
GtkWindow *mainwin;
#ifdef SHOW_SPLASH
GtkWidget *popup;
#endif

static void update_top(int which, int draww, int drawh, double xscal);
static void update_bottom(int which, int draww, int drawh);
/* It is almost - but not quite - worth integrating these two back into
update_foo().  It would also be nice to color encapsulated traffic correctly on
the total graph, but let's punt that for now. */
static void update_multi_top(int which, int draww, int drawh, double xscal);
static void update_multi_bottom(int which, int draww, int drawh);
static GtkWindow *create_win(int which, const char *tag);

// ignores args; they're from a version that tried to mmap them
static void *falloc(int id, unsigned long curtime) {
    void *rv = NULL;

    rv = calloc(2 * DEFTLEN * nsys, sizeof (unsigned long));
    if (rv == NULL) {
        fprintf(stderr, "mwmg: Couldn't allocate %zu bytes: %s\n",
            2 * DEFTLEN * nsys * sizeof (unsigned long), strerror(errno));
    }
    return rv;
}

/* Now even more inaptly named. */
static void blank(int which, int winno) {
    GdkPixmap *px;
    struct win *win;
    int i, y, end, incr;
    double xscal;
    double x;
    gint h;
    int w[2], hh[2];
    char t[6];

    end = M_LEFT;
    win = &wins[which];
    incr = TSINTER / (gauge ? 1 : interval) / win->sperpx;

    /* Don't draw a title, as the window is now titled. */
    gdk_draw_rectangle(win->dbs[winno], BLACK_GC(win->das[winno]), FALSE,
        M_LEFT, M_TOP, ALLOC_WIDTH(win->das[winno]) - M_LEFT - win->rm[winno],
        ALLOC_HEIGHT(win->das[winno]) - M_TOP - M_BOT);
    gdk_draw_rectangle(win->dbs[winno], WHITE_GC(win->das[winno]), TRUE,
        M_LEFT + 1, M_TOP + 1,
        ALLOC_WIDTH(win->das[winno]) - M_LEFT - win->rm[winno] - 2,
        ALLOC_HEIGHT(win->das[winno]) - M_TOP - M_BOT - 2);

    gdk_gc_set_foreground(win->gc, &gray);
    y = ALLOC_HEIGHT(win->das[winno]) - M_BOT;
    for (i = 0; i < NYMARKS; i++) { /* mark */
        gdk_draw_line(win->dbs[winno], win->gc, M_LEFT + 1, y,
            ALLOC_WIDTH(win->das[winno]) - win->rm[winno] - 1, y);
        y -= (ALLOC_HEIGHT(win->das[winno]) - M_TOP - M_BOT) / NYMARKS;
    }
    gdk_gc_set_foreground(win->gc, &win->col[0]);

#ifdef SHOW_STATISTICS
    if (which == W_CPU) {
        px = percent;
#ifdef REPORT_MEM
    } else if (which == W_MEM) {
        px = bytes;
#endif
#if 0
    } else if (which == W_TIME) {
        px = usec;
#endif
    } else {
        px = xpm;
    }
#else
    px = xpm;
#endif
    gdk_window_get_size(px, NULL, &h);
    gdk_draw_pixmap(win->dbs[winno], win->gc, px, 0, 0,
        ALLOC_WIDTH(win->das[winno]) - win->rm[winno] + 2,
        (ALLOC_HEIGHT(win->das[winno]) - M_TOP - M_BOT - h) / 2, -1, -1);

    gdk_gc_set_foreground(win->gc, &gray);
    if (!winno) {
        xscal = (ALLOC_WIDTH(win->das[0]) - M_LEFT - win->rm[0] - 2.0) / nsys;
        for (i = 1; i <= nsys; i++) {
            if (1 || !(i % 5)) {
                sprintf(t, "%d", i);
                gdk_draw_string(win->dbs[0], font, BLACK_GC(win->das[0]),
                    2 + M_LEFT + (i - 0.5) * xscal -
                    gdk_string_width(font, t) / 2,
                    ALLOC_HEIGHT(win->das[0]) - 2, t);
                gdk_draw_line(win->dbs[0], win->gc,
                    4 + M_LEFT + i * xscal - gdk_string_width(font, t) / 2,
                    M_TOP + 1,
                    4 + M_LEFT + i * xscal - gdk_string_width(font, t) / 2,
                    ALLOC_HEIGHT(win->das[0]) - M_BOT - 1);
            }
        }
    } else {
        for (i = incr; i < DEFTLEN / win->sperpx; i += 3 * incr) {
            gdk_draw_line(win->dbs[1], win->gc, M_LEFT + i, M_TOP + 1,
                M_LEFT + i, ALLOC_HEIGHT(win->das[1]) - M_BOT - 1);
            strftime(t, 6, "%R", &whens[i / incr - 1]);
            if (M_LEFT + i - gdk_string_width(font, t) / 2 >= end) {
                gdk_draw_rectangle(win->dbs[1], WHITE_GC(win->das[1]), TRUE,
                    M_LEFT + i - gdk_string_width(font, t) / 2,
                    ALLOC_HEIGHT(win->das[1]) - 2 - gdk_string_height(font, t),
                    gdk_string_width(font, t), gdk_string_height(font, t));
                gdk_draw_string(win->dbs[1], font, BLACK_GC(win->das[1]),
                    M_LEFT + i - gdk_string_width(font, t) / 2,
                    ALLOC_HEIGHT(win->das[1]) - 2, t);
                end = M_LEFT + i + gdk_string_width(font, t) / 2;
            }
        }
    }
    gdk_gc_set_foreground(win->gc, &win->col[0]);

    w[0] = ALLOC_WIDTH(wins[which].das[0]) - M_LEFT - wins[which].rm[0] - 2;
    w[1] = ALLOC_WIDTH(wins[which].das[1]) - M_LEFT - wins[which].rm[1] - 2;
    hh[0] = ALLOC_HEIGHT(wins[which].das[0]) - M_TOP - M_BOT - 1;
    hh[1] = ALLOC_HEIGHT(wins[which].das[1]) - M_TOP - M_BOT - 1;
    x = (double)w[0] / (double)nsys;

    if (/* (which == encaptgt) || */ (which == W_NET)) {
        if (!winno) {
            update_multi_top(which, w[0], hh[0], x);
        } else {
            update_multi_bottom(which, w[1], hh[1]);
        }
    } else if (!winno) {
        update_top(which, w[0], hh[0], x);
    } else {
        update_bottom(which, w[1], hh[1]);
    }
}

static gboolean delete_event(GtkWidget *w, GdkEvent *e, gpointer data) {
    gtk_main_quit();
    return FALSE;
}

static void expose_color(GtkWidget *w, GdkEventExpose *e, gpointer gp) {
    struct win *win = &wins[(long)gp / 2];
    int ofs = (long)gp % 2;

    if (win->gc == NULL) {
        win->gc = gdk_gc_new(w->window);
        /* if (which) */ gdk_gc_set_foreground(win->gc, &win->col[ofs]);
    }

    if (ofs) {
        gdk_gc_set_foreground(win->gc, &win->col[1]);
    }
    gdk_draw_rectangle(w->window, win->gc, TRUE, 0, 0, COLORSIZE, COLORSIZE);
    if (ofs) {
        gdk_gc_set_foreground(win->gc, &win->col[0]);
    }
}

static void toggle_event(GtkWidget *w, gpointer gp) {
    long which = (long)gp;

    if (showwins & (1 << which)) {
        gtk_widget_hide(GTK_WIDGET(wins[which].w));
    } else {
        gtk_widget_show(GTK_WIDGET(wins[which].w));
    }
    showwins ^= (1 << which);
}

static gboolean subwin_delete(GtkWidget *w, GdkEvent *e, gpointer gp) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gp), 0);
    return TRUE; /* I don't want to be destroyed. */
}

static GtkWindow *create_selector(char const *title) {
    GtkWidget *h, *qq;
    GtkWindow *rv;
    GtkBox *hbox, *vbox;
    char buf[256];
    int i;

    h = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    rv = GTK_WINDOW(h);
    snprintf(buf, 256, "Fortian %s Monitor",
        (title == NULL) ? "Network" : title);
    buf[255] = 0;
    gtk_window_set_title(rv, buf);
    gtk_window_set_policy(rv, FALSE, FALSE, TRUE);
    gtk_signal_connect(GTK_OBJECT(h), "delete_event",
        GTK_SIGNAL_FUNC(delete_event), NULL);

    h = gtk_vbox_new(TRUE, 0);
    vbox = GTK_BOX(h);
    gtk_container_add(GTK_CONTAINER(rv), h);
    gtk_widget_show(h);

    for (i = 0; i < nwins; i++) {
        wins[i].w = create_win(i, title);
#if 0
        if (i == W_NET) { /* Add a line between not traffic and traffic. */
            h = gtk_hseparator_new();
            gtk_table_attach(table, h, 0, 3, i, i + 1, GTK_EXPAND | GTK_FILL, 0,
                0, 0);
            gtk_widget_show(h);
        }
#endif
        if (i != W_NET) {
            h = gtk_toggle_button_new();

            qq = gtk_hbox_new(FALSE, 0);
            gtk_container_add(GTK_CONTAINER(h), qq);
            gtk_widget_show(qq);
            hbox = GTK_BOX(qq);

            qq = gtk_drawing_area_new();
            gtk_drawing_area_size(GTK_DRAWING_AREA(qq), COLORSIZE, COLORSIZE);
            gtk_box_pack_start(hbox, qq, FALSE, FALSE, 0);
            gtk_signal_connect(GTK_OBJECT(qq), "expose_event",
                GTK_SIGNAL_FUNC(expose_color), (gpointer)(long)(2 * i));
            gtk_widget_show(qq);

            if (1
#ifdef SHOW_STATISTICS
#ifdef REPORT_MEM
                && (i != W_MEM)
#endif
                && (i != W_CPU)
#endif
               ) {
                qq = gtk_drawing_area_new();
                gtk_drawing_area_size(GTK_DRAWING_AREA(qq),
                    COLORSIZE, COLORSIZE);
                gtk_box_pack_start(hbox, qq, FALSE, FALSE, 0);
                gtk_signal_connect(GTK_OBJECT(qq), "expose_event",
                    GTK_SIGNAL_FUNC(expose_color), (gpointer)(long)(2 * i + 1));
                gtk_widget_show(qq);
            }

            qq = gtk_label_new(wins[i].name);
            gtk_box_pack_start(hbox, qq, TRUE, TRUE, 3);
            gtk_widget_show(qq);
        } else {
            h = gtk_toggle_button_new_with_label(wins[i].name);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(h), showwins & (1 << i));
        gtk_signal_connect(GTK_OBJECT(h), "toggled",
            GTK_SIGNAL_FUNC(toggle_event), (gpointer)(long)i);
        gtk_signal_connect(GTK_OBJECT(wins[i].w), "delete_event",
            GTK_SIGNAL_FUNC(subwin_delete), (gpointer)(long)h);
        GTK_WIDGET_UNSET_FLAGS(h, GTK_CAN_FOCUS);
        gtk_box_pack_start_defaults(vbox, h);
        gtk_widget_show(h);

    }

    adj = gtk_adjustment_new(1, 1, DEFTLEN, 1, 10, 10);

    h = gtk_hscale_new(GTK_ADJUSTMENT(adj));
    gtk_box_pack_start_defaults(vbox, h);
    gtk_widget_set_sensitive(h, 0);
    gtk_scale_set_value_pos(GTK_SCALE(h), GTK_POS_LEFT);
    gtk_scale_set_digits(GTK_SCALE(h), 0);
    gtk_widget_show(h);

#if 0
    /* spinner is disabled if not replaying */
    h = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
    gtk_box_pack_start_defaults(vbox, h);
    gtk_widget_set_sensitive(h, replaying);
    gtk_widget_show(h);
#endif

    gtk_window_set_policy(rv, FALSE, FALSE, TRUE);
    gtk_widget_show(GTK_WIDGET(rv));
    return rv;
}

static gint configure_event(GtkWidget *w, GdkEventConfigure *e, gpointer gp) {
    int ofs = (long)gp / nwins;
    int which = (long)gp % nwins;
    struct win *win = &wins[which];
    GdkCursor *curs;

#include "bits.xpm"
#include "reqs.xpm"
#ifdef SHOW_STATISTICS
#include "bytes.xpm"
#include "percent.xpm"
    /* #include "usec.xpm" */
#endif

    curs = gdk_cursor_new(GDK_CROSS);
    gdk_window_set_cursor(w->window, curs);
    gdk_cursor_destroy(curs);

    if (win->dbs[ofs] != NULL) {
        gdk_pixmap_unref(win->dbs[ofs]);
    }
    win->dbs[ofs] = gdk_pixmap_new(w->window, e->width, e->height, -1);
    gdk_draw_rectangle(win->dbs[ofs], WHITE_GC(win->das[ofs]), TRUE, 0, 0,
        e->width, e->height);

    if (win->gc == NULL) {
        win->gc = gdk_gc_new(win->das[ofs]->widget.window);
        /* if (which) */ gdk_gc_set_foreground(win->gc, &win->col[0]);
    }

    if (xpm == NULL) {
#if 0
        xpm = gdk_pixmap_colormap_create_from_xpm_d(NULL,
            gtk_widget_get_colormap(w), NULL, NULL, bits_xpm);
#else
        xpm = gdk_pixmap_create_from_xpm_d(win->das[ofs]->widget.window, NULL,
            NULL, (curleg == E_BITS) ? bits_xpm : reqs_xpm);
#endif
    }
#ifdef SHOW_STATISTICS
    if (bytes == NULL) {
        bytes = gdk_pixmap_create_from_xpm_d(win->das[ofs]->widget.window, NULL,
            NULL, bytes_xpm);
    }
    if (percent == NULL) {
        percent = gdk_pixmap_create_from_xpm_d(win->das[ofs]->widget.window,
            NULL, NULL, percent_xpm);
    }
#endif
#if 0
    if (usec == NULL) {
        usec = gdk_pixmap_create_from_xpm_d(win->das[ofs]->widget.window, NULL,
            NULL, usec_xpm);
    }
#endif

    if (ofs) {
        /* Readjust sperpx and the right margin. */
        win->sperpx = ceil(DEFTLEN / (double)(e->width - M_LEFT-M_RIGHT-2));
        win->rm[1] = e->width - DEFTLEN / win->sperpx - M_LEFT - 2;
    }

    blank(which, ofs);

    return TRUE;
}

static void expose_event(GtkWidget *w, GdkEventExpose *e, gpointer gp) {
    int ofs = (long)gp / nwins;
    struct win *win = &wins[(long)gp % nwins];

    gdk_draw_pixmap(w->window, w->style->fg_gc[GTK_WIDGET_STATE(w)],
        win->dbs[ofs], e->area.x, e->area.y, e->area.x, e->area.y,
        e->area.width, e->area.height);

    if (0) {
        gint x, y;
        gdk_window_get_position(GTK_WIDGET(win->w)->window, &x, &y);
    }
}

static gboolean click_event(GtkWidget *w, GdkEventButton *e, gpointer gp) {
    char buf[256];
    struct win *win = &wins[(long)gp % nwins];
    long ofs = (long)gp / nwins;
    double v1, v2;

    if (e->button == 1) {
        gtk_statusbar_pop(win->sb, win->ctxid);
        if ((e->y > M_TOP) && (e->y <= ALLOC_HEIGHT(win->das[ofs]) - M_BOT)) {
            v1 = ALLOC_HEIGHT(win->das[ofs]) - e->y - M_BOT;
            v1 /= ALLOC_HEIGHT(win->das[ofs]) - M_TOP - M_BOT;
            v1 *= win->maxes[ofs];
            v2 = v1 + (double)win->maxes[ofs] /
                (ALLOC_HEIGHT(win->das[ofs]) - M_TOP - M_BOT);
            snprintf(buf, 255, "%s: %'g - %'g", win->name, v1, v2);
            gtk_statusbar_push(win->sb, win->ctxid, buf);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean win_config(GtkWidget *wi, GdkEventConfigure *e, gpointer data) {
    struct win *w = &wins[(long)data];

    if (!w->ncfg) {
        w->ncfg++;
        w->dx = e->x;
        w->dy = e->y;
    } else {
        if ((w->ncfg == 1) && e->send_event) { /* It's from the WM. */
            w->ncfg++;
            w->dx -= e->x;
            w->dy -= e->y;
        }
        w->cx = e->x + w->dx;
        w->cy = e->y + w->dy;
        w->cw = e->width;
        w->ch = e->height;
    }

    return FALSE;
}

/* This needs to be replaced; positions should move to the config file. */
static int getwinpos(int which, int *x, int *y, int *w, int *h) {
    FILE *f;
    char buf[BUFSIZ];
    int nr, hw;
    int rv = 0;

    if ((f = fopen(WINPOS, "r")) != NULL) {
        do {
            if (fgets(buf, BUFSIZ, f) != NULL) {
                buf[BUFSIZ - 1] = 0;
                /* I pondered using XParseGeometry but I really need hw. */
                if ((nr = sscanf(buf, "%d=%dx%d+%d+%d\n",
                    &hw, w, h, x, y)) == 5) { /* New geometry */
                    if (hw == which) {
                        fclose(f);
                        f = NULL;
                        rv = 1;
                        break;
                    }
                } else if ((nr = sscanf(buf, "%d=%d,%d\n", &hw, x, y)) == 3) {
                    if (hw == which) { /* Old geometry */
                        *w = *h = 0;
                        fclose(f);
                        f = NULL;
                        rv = 2;
                        break;
                    }
                }
            }
        } while (!(feof(f) || ferror(f)));
    }
    if (f != NULL) {
        fclose(f);
    }
    return rv;
}

static GtkWindow *create_win(int which, const char *tag) {
    GtkBox *vbox;
    GtkWidget *h;
    GtkWindow *rv;
    struct win *win = &wins[which];
    int x, y, rw, rh;
    char title[256];

    h = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    rv = GTK_WINDOW(h);
    if (tag != NULL) {
        snprintf(title, 256, "%s %s", win->name, tag);
    } else {
        strncpy(title, win->name, 256);
    }
    title[255] = 0;
    gtk_window_set_title(rv, title);
    gtk_window_set_policy(rv, FALSE, TRUE, FALSE);

    h = gtk_vbox_new(FALSE, 0);
    vbox = GTK_BOX(h);
    gtk_widget_show(h);
    gtk_container_add(GTK_CONTAINER(rv), h);

    h = gtk_drawing_area_new();
    win->das[0] = GTK_DRAWING_AREA(h);
    gtk_drawing_area_size(win->das[0], MINWIDTH + M_LEFT + win->rm[0],
        MINTHIGH + M_TOP + M_BOT);
    gtk_box_pack_start(vbox, h, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(h), "configure_event",
        GTK_SIGNAL_FUNC(configure_event), (gpointer)(long)which);
    gtk_signal_connect(GTK_OBJECT(h), "expose_event",
        GTK_SIGNAL_FUNC(expose_event), (gpointer)(long)which);
    gtk_signal_connect(GTK_OBJECT(h), "button_press_event",
        GTK_SIGNAL_FUNC(click_event), (gpointer)(long)which);
    gtk_widget_set_events(h, gtk_widget_get_events(h) | GDK_BUTTON_PRESS_MASK);
    gtk_widget_show(h);

    h = gtk_hseparator_new();
    gtk_box_pack_start(vbox, h, FALSE, TRUE, 0);
    gtk_widget_show(h);

    h = gtk_drawing_area_new();
    win->das[1] = GTK_DRAWING_AREA(h);
    gtk_drawing_area_size(win->das[1], MINWIDTH + M_LEFT + win->rm[1],
        MINBHIGH + M_TOP + M_BOT);
    gtk_box_pack_start(vbox, h, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(h), "configure_event",
        GTK_SIGNAL_FUNC(configure_event), (gpointer)(long)(which + nwins));
    gtk_signal_connect(GTK_OBJECT(h), "expose_event",
        GTK_SIGNAL_FUNC(expose_event), (gpointer)(long)(which + nwins));
    gtk_signal_connect(GTK_OBJECT(h), "button_press_event",
        GTK_SIGNAL_FUNC(click_event), (gpointer)(long)which + nwins);
    gtk_widget_set_events(h, gtk_widget_get_events(h) | GDK_BUTTON_PRESS_MASK);
    gtk_widget_show(h);

    h = gtk_statusbar_new();
    win->sb = GTK_STATUSBAR(h);
    gtk_box_pack_start(vbox, h, FALSE, FALSE, 0);
    gtk_widget_show(h);
    win->ctxid = gtk_statusbar_get_context_id(win->sb, win->name);


    gtk_widget_realize(GTK_WIDGET(rv));
    if (!getwinpos(which, &x, &y, &rw, &rh)) {
        x = y = 0;
        rw = 250;
        rh = 0;
    }
    gtk_widget_set_uposition(GTK_WIDGET(rv), x, y);
    /* gtk_window_reposition(GTK_WINDOW(rv), x, y); */
    gtk_window_move(rv, x, y);
    if (rw && rh) {
        gtk_window_resize(rv, rw, rh);
        gtk_window_set_default_size(rv, rw, rh);
    }
    gtk_signal_connect(GTK_OBJECT(rv), "configure_event",
        GTK_SIGNAL_FUNC(win_config), (gpointer)(long)which);

    if (showwins & (1 << which)) {
        gtk_widget_show(GTK_WIDGET(rv));
    }
    return rv;
}

#ifdef SHOW_SPLASH
static GtkWidget *create_popup(void) {
    GtkWidget *rv;
    GtkWidget *h;
    GtkWindow *win;
    GdkPixmap *splash;
    GdkBitmap *mask;

#include "mwmg.xpm"

    rv = gtk_window_new(GTK_WINDOW_POPUP);
    win = GTK_WINDOW(rv);
    gtk_window_set_title(win, "MWMG: Waiting for Data");
    gtk_window_set_position(win, GTK_WIN_POS_CENTER);

    splash = gdk_pixmap_colormap_create_from_xpm_d(NULL,
        gtk_widget_get_colormap(rv), &mask, NULL, mwmg_xpm);
    h = gtk_pixmap_new(splash, mask);
    gtk_container_add(GTK_CONTAINER(rv), h);
    gtk_widget_show(h);
    gtk_widget_show(rv);
    return rv;
}
#endif

double roundup(double x) {
    double rv = 1.0;
    int base;
    int i;

    for (base = 1; base < 13; base++) {
        rv *= 10;
        for (i = 1; i < 10; i++) {
            if (x <= (i * rv)) {
                return i * rv;
            }
        }
    }
    return i * rv; /* well, return it anyway */
}

double drawymarks(int which, int ofs, double max) {
    int i, y;
    double mar;
    char str[256];
    struct win *win = &wins[which];

#ifdef CLAMP_MAX
    if (!ofs) {
        max = TOPMAX;
    } else {
        max = BOTMAX;
    }
#else
    max = roundup(max);
#endif
    wins[which].maxes[ofs] = ceil(max);
#if 0
    if ((which == D_CPU) && (max > 100.0)) { /* clamp CPU graphs */
        max = 100.0;
    }
#endif
    gdk_draw_rectangle(win->dbs[ofs], WHITE_GC(win->das[ofs]), TRUE, 0, 0,
        M_LEFT, ALLOC_HEIGHT(win->das[ofs]));
    y = ALLOC_HEIGHT(win->das[ofs]) - M_BOT + cheight / 2 - 3;
    for (i = 0; i <= NYMARKS; i++) {
        mar = i * max / NYMARKS;
#ifdef SHOW_STATISTICS
        if (which == W_MEM) {
            if (mar < 1024.0) {
                snprintf(str, 256, "%4g", mar);
            } else if (mar < 1048576.0) {
                snprintf(str, 256, "%3.3f", mar / 1024.0);
                strcpy(&str[3], "K");
            } else if (mar < 1073741824.0) {
                snprintf(str, 256, "%3.3f", mar / 1048576.0);
                strcpy(&str[3], "M");
            } else {
                snprintf(str, 256, "%3.3fG", mar / 1073741824.0);
                strcpy(&str[3], "G");
            }
        } else {
#endif
            if (mar < 1000.0) {
                snprintf(str, 256, "%4g", mar);
            } else if (mar < 1000000.0) {
                snprintf(str, 256, "%3.3f", mar / 1000.0);
                strcpy(&str[3], "K");
            } else if (mar < 1000000000.0) {
                snprintf(str, 256, "%3.3f", mar / 1000000.0);
                strcpy(&str[3], "M");
            } else {
                snprintf(str, 256, "%3.3f", mar / 1000000000.0);
                strcpy(&str[3], "G");
            }
#ifdef SHOW_STATISTICS
        }
#endif
        str[255] = 0;
        gdk_draw_string(win->dbs[ofs], font, BLACK_GC(win->das[ofs]), 1, y,
            str);
        y -= (ALLOC_HEIGHT(win->das[ofs]) - M_TOP - M_BOT) / NYMARKS;
    }
    return max;
}

#if 0
void blockoff(int which, int sperpx) {
    struct win *win = &wins[which];

    /* Draw a new right side border at the correct distance over. */
    gdk_draw_line(win->dbs[1], BLACK_GC(win->das[1]), win->rm[1], M_TOP + 1,
        win->rm[1], ALLOC_HEIGHT(win->das[1]) - M_TOP - M_BOT - 2);
    gdk_draw_rectangle(win->dbs[1], WHITE_GC(win->das[1]), TRUE, win->rm[1] + 1,
        M_TOP + 1, ALLOC_WIDTH(win->das[1]) - win->rm[1] - 1,
        ALLOC_HEIGHT(win->das[1]) - M_TOP - M_BOT);
}
#endif

/* This is supposed to ignore aggregate data, by design. */
static void update_multi_top(int which, int draww, int drawh, double xscal) {
    double lmax, yscal;
    double ax[nwins];
    double memo;
    int i;
    int k;
    double s;
    int ofs;
    int sam;
    int cxtime;
    int delta = 1;
    int spacer;

    cxtime = curtime ? (curtime - 1) : 0;
    spacer = DEFTLEN * nsys;

    lmax = 0.0;
    for (i = 0; i < nsys; i++) {
        ax[0] = 0.0;
        for (k = W_OTHER; k < nwins; k++) {
            ax[0] += wins[k].samps[cxtime * nsys + i];
            ax[0] += wins[k].samps[cxtime * nsys + i + spacer];
        }
        if (lmax < ax[0]) {
            lmax = ax[0];
        }
    }
    if (!gauge) {
        lmax /= interval;
    }
    lmax = drawymarks(which, 0, lmax);

    for (k = W_OTHER; k < nwins; k++) {
        delta++;
    }
    yscal = ((drawh - delta) / lmax) / (gauge ? 1 : interval);
    ax[0] = 0.0;
    for (ofs = 0; ofs <= spacer; ofs += spacer) {
        for (i = 0; i < nsys; i++) {
            s = ALLOC_HEIGHT(wins[which].das[0]) - M_BOT;
            sam = cxtime * nsys + i + ofs;
            for (k = W_OTHER; k < nwins; k++) {
                ax[k] = wins[k].samps[sam];
            }

            memo = ceil(ax[0] * yscal);
            s -= memo;
            for (k = W_OTHER; k < nwins; k++) {
                s -= ceil(ax[k] * yscal);
            }

            /* this is the bounding rectangle */
            gdk_draw_rectangle(wins[which].dbs[0], wins[which].gc, TRUE,
                2 + M_LEFT + i * xscal, s, xscal - 1, memo);
            s += memo;
            for (k = W_OTHER; k < nwins; k++) {
                memo = wins[k].samps[sam];
                if (memo > 0.0) {
                    memo = ceil(memo * yscal);
                    if (ofs) {
                        gdk_gc_set_foreground(wins[k].gc, &wins[k].col[1]);
                    }
                    gdk_draw_rectangle(wins[which].dbs[0], wins[k].gc, TRUE,
                        2 + M_LEFT + i * xscal, s, xscal - 1, memo);
                    if (ofs) {
                        gdk_gc_set_foreground(wins[k].gc, &wins[k].col[0]);
                    }
                    s += memo;
                }
            }
        }
    }

    gdk_draw_pixmap(wins[which].das[0]->widget.window,
        FG_GC(wins[which].das[0])[GTK_WIDGET_STATE(wins[which].das[0])],
        wins[which].dbs[0], 0, 0, 0, 0, ALLOC_WIDTH(wins[which].das[0]),
        ALLOC_HEIGHT(wins[which].das[0]));
}

void update_top(int which, int draww, int drawh, double xscal) {
    int i;
    double lmax;
    double yscal;
    unsigned long cxtime;
    int dofs, iofs;
    int gb;
    struct win *win;

    cxtime = curtime ? curtime - 1 : 0;
    if (cxtime < 0) {
        cxtime = 0;
    }

    if ((which >= 0) && (which < nwins)) {
        win = &wins[which];
    } else {
        fprintf(stderr,
            "MWMG: critical: update_top called with which=%d,\n"
            "\tdraww=%d, drawh=%d, xscal=%g for bucket %lu\n",
            which, draww, drawh, xscal, cxtime);
        return;
    }

    xscal = (double)draww / nsys;
    gb = ALLOC_HEIGHT(win->das[0]) - M_BOT + 1;

    lmax = 0.0;
    for (i = 0; i < nsys; i++) {
        dofs = cxtime * nsys + i;
        iofs = dofs + nsys * DEFTLEN ;
        if (lmax < (win->samps[dofs] + win->samps[iofs])) {
            lmax = win->samps[dofs] + win->samps[iofs];
        }
    }
    if (!gauge) {
        lmax /= interval;
    }
    lmax = drawymarks(which, 0, lmax);

    yscal = (drawh / lmax) / (gauge ? 1 : interval);
    for (i = 0; i < nsys; i++) {
        dofs = cxtime * nsys + i;
        iofs = dofs + nsys * DEFTLEN;
        if (win->samps[dofs]) {
            gdk_gc_set_foreground(win->gc, &win->col[0]);
            gdk_draw_rectangle(win->dbs[0], win->gc, TRUE,
                2 + M_LEFT + i * xscal, gb - ceil(win->samps[dofs] * yscal),
                xscal - 1, ceil(win->samps[dofs] * yscal));
        }
        if (win->samps[iofs]) {
            gdk_gc_set_foreground(win->gc, &win->col[1]);
            gdk_draw_rectangle(win->dbs[0], win->gc, TRUE,
                2 + M_LEFT + i * xscal,
                gb - ceil((win->samps[dofs] + win->samps[iofs]) * yscal),
                xscal - 1, ceil(win->samps[iofs] * yscal));
            gdk_gc_set_foreground(win->gc, &win->col[0]);
        }
    }

    gdk_draw_pixmap(win->das[0]->widget.window,
        FG_GC(win->das[0])[GTK_WIDGET_STATE(win->das[0])], win->dbs[0],
        0, 0, 0, 0, ALLOC_WIDTH(win->das[0]), ALLOC_HEIGHT(win->das[0]));
}

void update_multi_bottom(int which, int draww, int drawh) {
    int i, j, m;
    int k;
    double s, ax[nwins], bx[nwins];
    double yscal, lmax;
    double memo;
    int delta;
    unsigned long h;
    int sperpx = wins[which].sperpx;
    int cxtime = curtime - 1;
    int ofs = DEFTLEN * nsys;

#if 0
    if (which != W_NET) {
        ofs = DEFTLEN * nsys;
    }
#endif

    if (cxtime < 0) {
        cxtime = 0;
    }

    lmax = 0.0;
    for (i = 0; i <= cxtime; i += sperpx) {
        bx[0] = ax[0] = 0.0;
        for (m = 0; m < nsys; m++) {
            for (j = 0; (j < sperpx) && (i + j <= cxtime); j++) {
                for (k = W_OTHER; k < nwins; k++) {
                    ax[0] += ceil((double)wins[k].samps[(i + j) * nsys + m] /
                        sperpx) / (gauge ? 1 : INTERVAL);
                    ax[0] += ceil((double)wins[k].samps[(i + j) *
                        nsys + m + ofs] / sperpx) / (gauge ? 1 : INTERVAL);
                }
            }
        }
        if (lmax < (ax[0] + bx[0])) {
            lmax = ax[0] + bx[0];
        }
    }

    lmax = drawymarks(which, 1, lmax);

#if 0
    for (k = W_OTHER; k < nwins; k++) {
        delta++; /* this is needlessly complex */
    }
#else
    delta = 1 + nwins - W_OTHER;
#endif
    yscal = (drawh - delta) / lmax;
    for (i = 0; i <= cxtime; i += sperpx) {
        s = ALLOC_HEIGHT(wins[which].das[1]) - M_BOT;
        memset(ax, 0, sizeof (double) * nwins);
        memset(bx, 0, sizeof (double) * nwins);
        for (j = 0; (j < sperpx) && (i + j <= cxtime); j++) {
            for (m = 0; m < nsys; m++) {
                for (k = W_OTHER; k < nwins; k++) {
                    h = wins[k].samps[(i + j) * nsys + m];
                    if (h) {
                        ax[k] += ceil((double)h / sperpx) / (gauge ? 1 : interval);
                    }
                    h = wins[k].samps[(i + j) * nsys + m + ofs];
                    if (h) {
                        bx[k] += ceil((double)h / sperpx) / (gauge ? 1 : interval);
                    }
                }
            }
        }

        for (k = W_OTHER; k < nwins; k++) {
            if (ax[k] || bx[k]) {
                memo = ceil((ax[k] + bx[k]) * yscal);
                s -= memo;
            }
        }

        /* this is the bounding line */
        gdk_draw_line(wins[which].dbs[1], wins[which].gc,
            M_LEFT + 1 + i / sperpx, s, M_LEFT + 1 + i / sperpx,
            ceil(s + ax[0] * yscal));
        s += ceil(ax[0] * yscal);
        for (k = W_OTHER; k < nwins; k++) {
            if (ax[k]) {
                gdk_gc_set_foreground(wins[k].gc, &wins[k].col[0]);
                gdk_draw_line(wins[which].dbs[1], wins[k].gc,
                    M_LEFT + 1 + i / sperpx, s, M_LEFT + 1 + i / sperpx,
                    ceil(s + ax[k] * yscal));
                s += ceil(ax[k] * yscal);
            }
            if (bx[k]) {
                gdk_gc_set_foreground(wins[k].gc, &wins[k].col[1]);
                gdk_draw_line(wins[which].dbs[1], wins[k].gc,
                    M_LEFT + 1 + i / sperpx, s, M_LEFT + 1 + i / sperpx,
                    ceil(s + bx[k] * yscal));
                s += ceil(bx[k] * yscal);
            }
        }
    }

    gdk_draw_pixmap(wins[which].das[1]->widget.window,
        FG_GC(wins[which].das[1])[GTK_WIDGET_STATE(wins[which].das[1])],
        wins[which].dbs[1], 0, 0, 0, 0, ALLOC_WIDTH(wins[which].das[1]),
        ALLOC_HEIGHT(wins[which].das[1]));
}

void update_bottom(int which, int draww, int drawh) {
    int i, j, m;
    double lmax = 0.0;
    double yscal;
    double ax, bx;
    unsigned reps;
    struct win *win = &wins[which];
    int sperpx = win->sperpx;
    int cxtime = curtime - 1;

    if (cxtime < 0) {
        cxtime = 0;
    }

    for (i = 0; i <= cxtime; i += sperpx) {
        ax = 0.0;
        reps = 0;
        for (j = 0; (j < sperpx) && (i + j <= cxtime); j++) {
            for (m = 0; m < nsys; m++) {
                ax += ((double)win->samps[(i + j) * nsys + m] / sperpx) /
                    (gauge ? 1 : interval);
                ax += ((double)win->samps[(i + j + DEFTLEN) * nsys + m] /
                    sperpx) / (gauge ? 1 : interval);
#ifdef SHOW_STATISTICS
                if (((which == W_CPU) /* || (which == W_MEM) ||
                                         (which == W_TIME) */) && win->samps[(i + j) * nsys + m]) {
                    reps++;
                }
#endif
            }
        }
        if (reps) {
            ax /= (double)reps / sperpx;
        }
        if (lmax < ax) {
            lmax = ax;
        }
    }

    lmax = drawymarks(which, 1, lmax);
    yscal = drawh / lmax;
    for (i = 0; i <= cxtime; i += sperpx) {
        ax = 0.0;
        bx = 0.0;
        reps = 0;
        for (j = 0; (j < sperpx) && (i + j <= cxtime); j++) {
            for (m = 0; m < nsys; m++) {
                ax += ((double)win->samps[(i + j) * nsys + m] / sperpx) /
                    (gauge ? 1 : interval);
                bx += ((double)win->samps[(i + j + DEFTLEN) * nsys + m] /
                    sperpx) / (gauge ? 1 : interval);
#ifdef SHOW_STATISTICS
                if (((which == W_CPU) /* || (which == W_MEM) ||
                                         (which == W_TIME) */) && win->samps[(i + j) * nsys + m]) {
                    reps++;
                }
#endif
            }
        }
        if (reps) {
            ax /= (double)reps / sperpx;
            bx /= (double)reps / sperpx;
        }
        if (ax > 0.0) {
            gdk_gc_set_foreground(win->gc, &win->col[0]);
            gdk_draw_line(win->dbs[1], win->gc, M_LEFT + 1 + i / sperpx,
                ALLOC_HEIGHT(win->das[1]) - M_BOT - ceil(ax * yscal),
                M_LEFT + 1 + i / sperpx, ALLOC_HEIGHT(win->das[1]) - M_BOT);
        }
        if (bx > 0.0) {
            gdk_gc_set_foreground(win->gc, &win->col[1]);
            gdk_draw_line(win->dbs[1], win->gc, M_LEFT + 1 + i / sperpx,
                ALLOC_HEIGHT(win->das[1]) - M_BOT - ceil((ax + bx) * yscal),
                M_LEFT + 1 + i / sperpx,
                ALLOC_HEIGHT(win->das[1]) - M_BOT - ceil(ax * yscal));
        }
    }

    gdk_draw_pixmap(win->das[1]->widget.window,
        FG_GC(win->das[1])[GTK_WIDGET_STATE(win->das[1])], win->dbs[1],
        0, 0, 0, 0, ALLOC_WIDTH(win->das[1]), ALLOC_HEIGHT(win->das[1]));
}

gint update(gpointer data) {
    int i;
    /* int w[2], h[2]; */
    /* double x; */
    int showtime = 1;

    if (showtime) {
        for (i = 0; i < nwins; i++) {
            if (showwins & (1 << i)) {

#if 0 /* moved to blank() */
                w[0] = ALLOC_WIDTH(wins[i].das[0]) - M_LEFT - wins[i].rm[0] - 2;
                w[1] = ALLOC_WIDTH(wins[i].das[1]) - M_LEFT - wins[i].rm[1] - 2;
                h[0] = ALLOC_HEIGHT(wins[i].das[0]) - M_TOP - M_BOT - 1;
                h[1] = ALLOC_HEIGHT(wins[i].das[1]) - M_TOP - M_BOT - 1;
                x = (double)w[0] / (double)nsys;
#endif

                blank(i, 0);
                blank(i, 1);

#if 0 /* moved to blank() */
                if (i != W_NET) {
                    update_top(i, w[0], h[0], x);
                    update_bottom(i, w[1], h[1]);
                } else {
                    update_agg_top(w[0], h[0], x);
                    update_agg_bottom(w[1], h[1]);
                }
#endif

            }
#if 0
            if ((i != W_NET) && (msync(wins[i].samps,
                        DEFTLEN * nsys * sizeof (unsigned long), MS_ASYNC) < 0)) {
                fprintf(stderr, "mwmg: Couldn't synchronize for '%s': %s\n",
                    wins[i].name, strerror(errno));
            }
#endif
        }

        curtime++; /* XXX We don't charge packets to the stamped time. */
        gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), curtime);
#if 0
        printf("\r            MWMG Current Time Offset: %d", curtime);
        fflush(stdout);
#endif
    }

    return (curtime < DEFTLEN);
}

#if 0
static void doremote(gpointer data, gint source, GdkInputCondition condition) {
    char line[BUFSIZ];
    int rv;

    rv = read(source, line, BUFSIZ - 1);
    if (rv < 0) {
        fprintf(stderr, "mwmg: Couldn't read: %s\n", strerror(errno));
    } else {
        line[rv] = 0;
    }
}
#endif

static void readin(gpointer data, gint source, GdkInputCondition condition) {
    char line[BUFSIZ];
    int who;
    time_t inter, i;
    int flow;
    unsigned long incr;
    char *idx;
    int encapped;
    guint *gta = (guint *)data;
    static int done = 0;
    unsigned long totlen = 2 * DEFTLEN * nsys * sizeof (unsigned long);

    if (!done) {
        /* Since we got triggered, there must be input. */
#ifdef SHOW_SPLASH
        gtk_widget_hide(popup);
#endif
        *gta = gtk_timeout_add(interval * 1000, update, NULL);

        start = time(NULL);
        for (i = start /* + TSINTER */; i < start + DEFTLEN; i += TSINTER) {
            inter = i;
            localtime_r(&inter, &whens[(i - start) / TSINTER - 1]);
        }
        done++;
    }

    errno = 0;
    if ((curtime >= DEFTLEN) || (fgets(line, BUFSIZ, stdin) == NULL)) {
#if defined(DEBUG) && ((__SIZEOF_LONG__ == __SIZEOF_WCHAR_T) || (__SIZEOF_LONG__ == 4) || (__SIZEOF_LONG__ == 8))
        if (errno) {
            perror("MWMG pipeline read");
#ifdef DEBUG
        } else if (curtime == DEFTLEN) { /* only do this once */
            for (incr = 0, i = 0; i < totlen; i++) {
                if (wins[W_NET].samps[i] !=
#if (__SIZEOF_LONG__ == __SIZEOF_WCHAR_T) || (__SIZEOF_LONG__ == 4)
                    0xdeadbeef
#else
                    0xdeadbeefdeadbeef
#endif
                   ) {
                    incr++;
                }
            }
            if (incr < totlen) {
                fprintf(stderr, "As of %lu, only %lu samples of %lu were initialized.\n", curtime, incr, totlen);
            }
#endif
        }
#endif

#ifndef RUN_FOREVER
        gtk_main_quit();
#endif
        return;
    }

    line[BUFSIZ - 1] = 0;

    /* relies on short-circuit evaluation */
    if (((idx = strtok(line, ":")) == NULL) ||
        (((i = atoi(idx)) < start) || ((idx = strtok(NULL, ":")) == NULL)) ||
        (((who = atoi(idx)) <= 0) || ((idx = strtok(NULL, ":")) == NULL))) {
        return;
    }

    if (idx[0] == 'X') {
        flow = W_OTHER;
#ifdef SHOW_STATISTICS
    } else if (idx[0] == 'C') {
        flow = W_CPU;
#ifdef REPORT_MEM
    } else if (idx[0] == 'M') {
        flow = W_MEM;
#endif
#endif
    } else {
        flow = atoi(idx) + W_USER - 1;
    }

    if ((idx = strtok(NULL, ":")) == NULL) {
        return;
    }
    incr = strtoul(idx, NULL, 10);

    if ((idx = strtok(NULL, "\n")) == NULL) {
        return;
    }
    encapped = (*idx != '0');

    i -= start;
    /* only update if we have time left */
    who--; /* the collector outputs 1-based */
    wins[flow].samps[i * nsys + who + encapped * DEFTLEN * nsys] += incr;
#if 0
    msync(wins[flow].samps, nsys * DEFTLEN * sizeof (unsigned long), MS_ASYNC);
#endif
}

int main(int argc, char *argv[]) {
    guint gta;
    int i;
    time_t inter;
#if 0
    gint gremote;
    struct sockaddr_un sun;
#endif
    FILE *f;
    char line[256];
    struct win *h;
    char *idx;
    int x, focus, ofs;
    char *cfg = CONFFILE;
    char *tag = NULL;
    int badflag = 0;

    puts("Utilization Monitor (built on " BUILD_DATE " by " BUILD_USER ")");
    puts("Developed 2008-2019 by Fortian Inc.\n");

    /* gtk_set_locale(); */
    gtk_init(&argc, &argv);

    for (i = 1; (i < argc) && !badflag; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'n') {
                if (argv[i][2]) {
                    nsys = atoi(&argv[i][2]);
                } else if (argc > ++i) {
                    nsys = atoi(argv[i]);
                } else {
                    nsys = 0;
                }
                if (!nsys) {
                    fprintf(stderr,
                        "%s: -n requires a positive numeric argument\n",
                        argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'd') {
                idx = NULL;
                if (argv[i][2]) {
                    idx = &argv[i][2];
                } else if (argc > ++i) {
                    idx = argv[i];
                }
                if (idx == NULL) {
                    fprintf(stderr, "%s: -d requires a directory name\n",
                        argv[0]);
                    badflag = __LINE__;
                } else if (chdir(idx) < 0) {
                    fprintf(stderr,
                        "%s: couldn't change directory to '%s': %s\n",
                        argv[0], idx, strerror(errno));
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'c') {
                cfg = NULL;
                if (argv[i][2]) {
                    cfg = &argv[i][2];
                } else if (argc > ++i) {
                    cfg = argv[i];
                }
                if (cfg == NULL) {
                    fprintf(stderr,
                        "%s: -c requires a configuration file name\n", argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'g') {
                gauge++;
            } else if (argv[i][1] == 'i') {
                if (argv[i][2]) {
                    interval = atoi(&argv[i][2]);
                } else if (argc > ++i) {
                    interval = atoi(argv[i]);
                } else {
                    interval = 0;
                }
                if (interval <= 0) {
                    fprintf(stderr, "%s: -i requires a non-negative number\n",
                        argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'u') {
                idx = NULL;
                if (argv[i][2]) {
                    idx = &argv[i][2];
                } else if (argc > ++i) {
                    idx = argv[i];
                }
                if ((idx == NULL) || (strcmp(idx, "bits") &&
                    strcmp(idx, "reqs") && strcmp(idx, "requests"))) {
                    fprintf(stderr,
                        "%s: -u requires \"bits\", \"reqs\", or \"requests\"\n",
                        argv[0]);
                    badflag = __LINE__;
                } else if (strcmp(idx, "bits")) {
                    curleg = E_REQS;
                }
            } else if (argv[i][1] == 'w') {
                idx = NULL;
                if (isdigit(argv[i][2])) {
                    idx = &argv[i][2];
                } else if ((argc > ++i) && isdigit(argv[i][0])) {
                    idx = argv[i];
                }
                if (idx != NULL) {
                    showwins = strtol(argv[i], NULL, 0);
                } else {
                    fprintf(stderr, "%s: -w requires a numeric argument\n",
                        argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 't') {
                if (argv[i][2]) {
                    tag = &argv[i][2];
                } else if (argc > ++i) {
                    tag = argv[i];
                } else {
                    fprintf(stderr, "%s: -t requires a string\n", argv[0]);
                    badflag = __LINE__;
                }
            } else {
                fprintf(stderr, "%s: Unknown flag %s\n", argv[0], argv[i]);
            }
        } else {
            fprintf(stderr, "%s: Unknown argument %s\n", argv[0], argv[i]);
        }
    }
    if (badflag) {
        printf(line, "Usage: %s [-c CONFIG] [-d RUNDIR] [-g] [-i INTERVAL]\n",
            argv[0]);
        puts(               "     [-n NSYS] [-t TITLE]     [-u UNITS] [-w WINSPEC]");
        puts("    CONFIG is the fully qualified filename of the MWMG configuration file");
        puts("        (default: " CONFFILE " in the current working directory)");
        puts("    RUNDIR is an existing directory in which to store output files");
        puts("        (currently ignored)");
        puts("    -g enables \"gauge\" mode: data points are absolutes instead of deltas");
        puts("    INTERVAL is the number of seconds between sweeps");
        puts("    NSYS is the number of systems to be graphed, and must be positive");
        puts("        (default: " S(DEFNSYS) ")");
        puts("    TITLE is a name to include in the name of all windows, such "
            "as \"Network\"");
        puts("    UNITS is one of: 'bits' 'reqs' 'requests' (default: bits)");
        return 1;
    }

    wins = calloc(W_USER, sizeof (struct win));
    if (wins == NULL) {
        fprintf(stderr, "%s: Couldn't allocate window buffer: %s",
            argv[0], strerror(errno));
        return 3;
    }

    font = gdk_font_load(FONT);
    cwidth = gdk_char_width(font, 'X');
    cheight = font->ascent + font->descent + 1;

#ifdef SHOW_STATISTICS
#ifdef REPORT_MEM
    wins[W_MEM].name = strdup("Memory Utilization");
    wins[W_MEM].samps = falloc(W_MEM, curtime);
    wins[W_MEM].rm[0] = wins[W_MEM].rm[1] = M_RIGHT;
    wins[W_MEM].sperpx = 1;
#endif
    wins[W_CPU].name = strdup("Processor Utilization");
    wins[W_CPU].samps = falloc(W_CPU, curtime);
    wins[W_CPU].rm[0] = wins[W_CPU].rm[1] = M_RIGHT;
    wins[W_CPU].sperpx = 1;
#endif
#if 0
    wins[W_TIME].name = strdup("Delay");
    wins[W_TIME].samps = falloc(W_TIME, curtime);
    wins[W_TIME].rm[0] = wins[W_TIME].rm[1] = M_RIGHT;
    wins[W_TIME].sperpx = 1;
#endif

    wins[W_NET].name = strdup("Aggregate");
    wins[W_NET].samps = falloc(W_NET, curtime); /* This might never get touched. */
#if defined(DEBUG) && defined(__SIZEOF_LONG__)
#if defined(__SIZEOF_WCHAR_T__) && (__SIZEOF_LONG__ == __SIZEOF_WCHAR_T__)
    wmemset((wchar_t *)wins[W_NET].samps, 0xdeadbeef, 2 * DEFTLEN * nsys);
#elif (__SIZEOF_LONG__ == 4) || (__SIZEOF_LONG__ == 8)
#if __SIZEOF_LONG__ == 4
    wins[W_NET].samps[0] = 0xdeadbeef;
#elif __SIZEOF_LONG__ == 8
    wins[W_NET].samps[0] = 0xdeadbeefdeadbeef;
#endif
    for (i = 1; i < 2 * DEFTLEN * nsys; i++) {
        memcpy(&wins[W_NET].samps[i], wins[W_NET].samps, sizeof (long));
    }
#else
#warning "Not filling memory, sizeof (samps[0]) is neither 4 nor 8"
#endif
#endif
    gdk_color_black(gdk_colormap_get_system(), &wins[W_NET].col[0]);
    wins[W_NET].rm[0] = wins[W_NET].rm[1] = M_RIGHT;
    wins[W_NET].sperpx = 1;

#if 1
    wins[W_OTHER].name = strdup("Unknown");
#else
    wins[W_OTHER].name = strdup("Background");
#endif
    wins[W_OTHER].samps = falloc(W_OTHER, curtime);
    gdk_color_black(gdk_colormap_get_system(), &wins[W_OTHER].col[0]);
    wins[W_OTHER].col[1].red = wins[W_OTHER].col[1].green =
        wins[W_OTHER].col[1].blue = 0x3000;
    if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
            &wins[W_OTHER].col[1], FALSE, TRUE)) {
        fprintf(stderr, "%s: Couldn't allocate color for relayed unknown!\n",
            argv[0]);
        return 5;
    }
    wins[W_OTHER].rm[0] = wins[W_OTHER].rm[1] = M_RIGHT;
    wins[W_OTHER].sperpx = 1;

    gray.red = gray.green = gray.blue = 0xC000;
    if (!gdk_colormap_alloc_color(gdk_colormap_get_system(), &gray, 0, 1)) {
        fprintf(stderr, "%s: Couldn't allocate gray!", argv[0]);
        return 4;
    }

    f = fopen(cfg, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: Couldn't read '%s': %s\n",
            argv[0], cfg, strerror(errno));
        return 2;
    }

    nwins = W_USER;
    while (!feof(f)) {
        errno = 0;
        if (fgets(line, 255, f) == NULL) {
            if (errno) {
                fprintf(stderr, "%s: error while reading %s: %s\n",
                    argv[0], cfg, strerror(errno));
                return 7;
            } /* otherwise it was just EOF and no characters were read */
            break;
        }
        line[255] = 0;
        x = -1;
        focus = -1;
        ofs = -1;
        idx = strrchr(line, '\n');
        if (idx != NULL) *idx = 0;
        idx = strchr(line, ';');
        if (idx != NULL) *idx = 0;
        if (line[0] == '[') {
            h = realloc(wins, (nwins + 1) * sizeof (struct win));
            if (h == NULL) {
                fprintf(stderr, "%s: Couldn't reallocate window buffer: %s",
                    argv[0], strerror(errno));
                fclose(f);
                return 5; /* die messily */
            }
            wins = h;
            memset(&wins[nwins], 0, sizeof (struct win));
            idx = strrchr(line, ']');
            if (idx == NULL) {
                fprintf(stderr, "%s: '%s' is malformed.\n", argv[0], line);
                fclose(f);
                return 6;
            }
            *idx = 0;
            wins[nwins].name = strdup(&line[1]);
            wins[nwins].samps = falloc(nwins, curtime);
            wins[nwins].rm[0] = wins[nwins].rm[1] = M_RIGHT;
            wins[nwins].sperpx = 1;
            nwins++;
        } else if (!strncmp(line, "Memory=", 7)) {
#if (defined(SHOW_STATISTICS) && defined(REPORT_MEM))
            ofs = 7;
            x = 0;
            focus = W_MEM;
#endif
        } else if (!strncmp(line, "Processor=", 10)) {
#ifdef SHOW_STATISTICS
            ofs = 10;
            x = 0;
            focus = W_CPU;
#endif
        } else if (!strncmp(line, "Delay=", 6)) {
#if 0
            ofs = 6;
            x = 0;
            focus = W_TIME;
#endif
        } else if (!strncmp(line, "Color=", 6)) {
            ofs = 6;
            x = 0;
            focus = nwins - 1;
        } else if (!strncmp(line, "Relay=", 6)) {
            ofs = 6;
            x = 1;
            focus = nwins - 1;
#ifdef ENCAPS
        } else if (!strncmp(line, "Encapsulate=", 12)) {
            /* encaptgt = nwins - 1; */
#endif
        }
        if (x >= 0) {
            if (gdk_color_parse(&line[ofs], &wins[focus].col[x])) {
                if (!gdk_colormap_alloc_color(gdk_colormap_get_system(),
                        &wins[focus].col[x], FALSE, TRUE)) {
                    fprintf(stderr, "%s: Couldn't allocate %s for %s graph\n",
                        argv[0], &line[ofs], wins[focus].name);
                }
            } else {
                fprintf(stderr, "%s: Couldn't parse %s for %s graph\n",
                    argv[0], &line[ofs], wins[focus].name);
            }
        }
    }
    fclose(f);
    f = NULL;

#ifdef SHOW_SPLASH
    popup = create_popup();
#endif

    start = time(NULL);
    whens = calloc(DEFTLEN / TSINTER, sizeof (struct tm));
    for (i = start + TSINTER; i < start + DEFTLEN; i += TSINTER) {
        inter = i;
        localtime_r(&inter, &whens[(i - start) / TSINTER - 1]);
    }

    mainwin = create_selector(tag);

    /* ginp = */ gdk_input_add(0, GDK_INPUT_READ, readin, &gta);

#if 0 /* unix socket stuff */
    gremote = socket(PF_UNIX, SOCK_DGRAM, 0);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, REMOTE);
    unlink(REMOTE);
    if (bind(gremote, (struct sockaddr *)&sun,
            sizeof (struct sockaddr_un)) < 0) {
        perror("bind");
    } else {
        gdk_input_add(gremote, GDK_INPUT_READ, doremote, NULL);
    }
#endif

    gtk_main();

#if 0
    close(gremote);
#endif

    f = fopen(WINPOS, "w");
    if (f == NULL) {
        fprintf(stderr, "%s: Couldn't save window positions to " WINPOS ": %s",
            argv[0], strerror(errno));
    }

    free(whens);
    for (i = 0; i < nwins; i++) {
        free(wins[i].samps);
        if (f != NULL) {
#if 0
            fprintf(f, "%d=%d,%d\n", i, wins[i].cx, wins[i].cy);
#else
            fprintf(f, "%d=%dx%d+%d+%d\n",
                i, wins[i].cw, wins[i].ch, wins[i].cx, wins[i].cy);
#endif
        }
        free(wins[i].name);
    }
    free(wins);
    if (f != NULL) {
        fclose(f);
    }

    gdk_font_unref(font);
    gdk_pixmap_unref(xpm);
#ifdef SHOW_STATISTICS
    gdk_pixmap_unref(bytes);
    gdk_pixmap_unref(percent);
#endif
#if 0
    gdk_pixmap_unref(usec);
#endif

    return 0;
}
