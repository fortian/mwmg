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

#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "ftfm.h"

#define LINELEN 32
#define NAMELEN 4
#define CONFFILE "collector.conf"

pthread_cond_t waitconn = PTHREAD_COND_INITIALIZER;
pthread_mutex_t connlock = PTHREAD_MUTEX_INITIALIZER;
fd_set needconn; /* this is actually just used as a big bitfield */
fd_set bigfs;
int lastsock = 0;

struct ssys {
    char s[NAMELEN + 1];
    struct sockaddr_in sin;
    int sock;
    int mode;
} *sys = NULL;
int nsys = 0;

struct flow *flows = NULL;
int nflows = 0;

static void addconn(char *s) {
    char *id, *ip, *port;
    struct flow *fh = NULL;
    struct sockaddr_in sin;
    struct ssys *h;
    char *idx, *hyp;
    int sock = -1;
    int mgen = 0; /* XXX reuses fields/flags, assumes USE_MGEN is always set */
#ifdef TRY_SETSOCKOPT
    struct linger l = { 0, 0 };
    int one = 1;
#endif
    static int ln = 0;

    ln++;
    idx = strchr(s, '\n');
    if (idx != NULL) *idx = 0;
    idx = strchr(s, '#');
    if (idx != NULL) *idx = 0;

    sin.sin_family = AF_INET;
    if ((idx = strchr(s, '=')) != NULL) {
        *idx++ = 0;
        if (*idx == 'M') { /* It's an MGEN flowid (otherwise it's a port). */
            mgen = 1;
            idx++;
        }
        if (nflows < 64) { /* XXX We can only look for 64 flows right now. */
            fh = realloc(flows, (nflows + 1) * sizeof (struct flow));
        }
        if (fh != NULL) {
            flows = fh;
            if ((fh[nflows].id = atoi(s)) == 0) {
                fprintf(stderr, "recollect: couldn't parse ID on line %d.\n",
                    ln);
                /* The next realloc will get the same handle, or we'll leak one
                   flow's worth of memory.  C'est la guerre. */
            } else if ((fh[nflows].mgenid[0] = atol(idx)) == 0) {
                fprintf(stderr, "recollect: couldn't parse start on line %d.\n",
                    ln);
            } else { /* We have the start of a good line here. */
                if ((hyp = strchr(idx, '-')) == NULL) {
                    fh[nflows].mgenid[1] = fh[nflows].mgenid[0];
                } else {
                    if ((fh[nflows].mgenid[1] = atol(++hyp)) == 0) {
                        fprintf(stderr,
                            "recollect: couldn't parse end on line %d.\n", ln);
                        fh[nflows].mgenid[0] = 0;
                    }
                }
                if (fh[nflows].mgenid[0]) { /* the line parsed okay */
                    if (!mgen) {
                        fh[nflows].port[0] = fh[nflows].mgenid[0] & 0xFFFF;
                        fh[nflows].port[1] = fh[nflows].mgenid[1] & 0xFFFF;
                        fh[nflows].mgenid[0] = fh[nflows].mgenid[1] = 0;
                    } else {
                        fh[nflows].port[0] = fh[nflows].port[1] = 0;
                    }
                    nflows++;
                } /* else just lose this, as before; presumably we complained */
            }
        } else {
            fprintf(stderr,
                "recollect: couldn't reallocate flow groups on line %d: %s\n",
                ln, strerror(errno));
        }
    } else if (((id = strtok(s, ":")) != NULL) &&
        ((ip = strtok(NULL, ":")) != NULL) &&
        ((port = strtok(NULL, "\n")) != NULL) && *port &&
        inet_aton(ip, &sin.sin_addr) && (sin.sin_port = htons(atoi(port))) &&
        ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
#ifdef TRY_SETSOCKOPT
        /* These all consistently failed the last time I tried them under
        Linux, so they're currently disabled.  You're welcome to try turning
        them on again; they shouldn't hurt. */
        if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&l,
            sizeof (struct linger)) < 0) {
            perror("Ignoring error setting socket option SO_LINGER");
        }
        if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&one,
            sizeof (int)) < 0) {
            perror("Ignoring error setting socket option SO_KEEPALIVE");
        }
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
            sizeof (int)) < 0) {
            perror("Ignoring error setting socket option SO_REUSEADDR");
        }
#endif

        h = realloc(sys, (nsys + 1) * sizeof (struct ssys));
        if (h == NULL) {
            perror("recollect: couldn't reallocate serverlist buffer");
            close(sock);
        } else {
            if (sock > lastsock) {
                lastsock = sock;
            }
            strncpy(h[nsys].s, id, NAMELEN);
            h[nsys].s[NAMELEN - 1] = 0;
            h[nsys].sock = sock;
            h[nsys].mode = MODE_SRC;
            memcpy(&h[nsys].sin, &sin, sizeof (struct sockaddr_in));
            FD_SET(nsys, &needconn);
            sys = h;
            nsys++;
        }
    } else if (sock < 0) {
        perror("recollect: couldn't create socket");
    } else {
        fprintf(stderr, "Line %d of configuration file is incorrect: `%s'\n",
            ln, s);
    }
}

void *conner(void *vname) {
    int i;
    fd_set fs;
    char *prog;
    struct timespec ts;
    int alreadyfailed = 0;

    prog = (char *)vname;
    fprintf(stderr, "%s: connecting to hosts...\n", prog);
    fflush(stderr);
    pthread_mutex_lock(&connlock);

#if 0
    /* try once with the big fd_set locked, then fall back to the background */
    for (i = 0; i < nsys; i++) {
        if (FD_ISSET(i, &needconn)) {
            fprintf(stderr, "%s: connecting to %s ...", prog, sys[i].s);
            fflush(stderr);
            if (connect(sys[i].sock, (struct sockaddr *)&sys[i].sin,
                sizeof (struct sockaddr_in)) < 0) {
                fprintf(stderr,
                    "\r\033[2KError connecting to %s:%u as system %s: %s\n",
                    inet_ntoa((struct in_addr)sys[i].sin.sin_addr),
                    ntohs(sys[i].sin.sin_port), sys[i].s,
                    strerror(errno));
            } else {
                fprintf(stderr, "\r\033[2K%s: Connected to %s:%u (%s)\n",
                    prog, inet_ntoa((struct in_addr)sys[i].sin.sin_addr),
                    ntohs(sys[i].sin.sin_port), sys[i].s);
                fflush(stderr);
                FD_SET(sys[i].sock, &bigfs);
                FD_CLR(i, &needconn);
            }
        }
    }
#endif

    for (;;) {
        memcpy(&fs, &needconn, sizeof (fd_set));
        pthread_mutex_unlock(&connlock);

        for (i = 0; i < nsys; i++) {
            if (FD_ISSET(i, &fs)) {
                fprintf(stderr, "%s: connecting to %s ...", prog, sys[i].s);
                fflush(stderr);
                if (connect(sys[i].sock, (struct sockaddr *)&sys[i].sin,
                    sizeof (struct sockaddr_in)) < 0) {
                    fprintf(stderr, "\r\033[2K%s: error connecting to %s:%u as system %s: %s\n",
                        prog, inet_ntoa((struct in_addr)sys[i].sin.sin_addr),
                        ntohs(sys[i].sin.sin_port), sys[i].s, strerror(errno));
                } else {
                    fprintf(stderr, "\r\033[2K%s: Connected to %s:%u (%s)\n",
                        prog, inet_ntoa((struct in_addr)sys[i].sin.sin_addr),
                        ntohs(sys[i].sin.sin_port), sys[i].s);
                    fflush(stderr);
                    pthread_mutex_lock(&connlock);
                    FD_SET(sys[i].sock, &bigfs);
                    FD_CLR(i, &needconn);
                    pthread_mutex_unlock(&connlock);
                }
            }
        }
        fprintf(stderr, "%s: waiting 10 seconds before looping again...\n",
            prog);
        pthread_mutex_lock(&connlock);
        if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
            if (!alreadyfailed) {
                fprintf(stderr, "%s: clock_gettime: %s\n",
                    prog, strerror(errno));
                alreadyfailed++;
            }
            pthread_cond_wait(&waitconn, &connlock);
        } else {
            ts.tv_sec += 10;
            pthread_cond_timedwait(&waitconn, &connlock, &ts);
        }
    }
    pthread_mutex_unlock(&connlock);
    return NULL;
}

int clobber(int i) {
    shutdown(sys[i].sock, SHUT_RDWR);
    close(sys[i].sock);
    pthread_mutex_lock(&connlock);
    FD_CLR(sys[i].sock, &bigfs);
    pthread_mutex_unlock(&connlock);
    if ((sys[i].sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "recollect: couldn't create socket for %s (dropping): %s\n",
            sys[i].s, strerror(errno));
        return 1;
    }
    if (sys[i].sock > lastsock) {
        lastsock = sys[i].sock;
    }
    pthread_mutex_lock(&connlock);
    FD_SET(i, &needconn);
    pthread_mutex_unlock(&connlock);
    return 0;
}

uint64_t findflow(uint16_t p
#ifdef USE_MGEN
    , uint32_t m
#endif
    ) {
    uint64_t rv = 0;
    int i;

    for (i = 0; i < nflows; i++) {
        if ((p && (flows[i].port[0] <= p) && (flows[i].port[1] >= p))
#ifdef USE_MGEN
            || (m && (flows[i].mgenid[0] <= m) && (flows[i].mgenid[1] >= m))
#endif
            ) {
            rv |= (1 << i);
        }
    }
    return rv;
}

int consume(int i) { /* Format change on 2015 08 07 */
    struct wire otw;
    uint64_t ffr;
    int k;
    time_t when;
    struct tm *t;
    static uint32_t last = 0;

    if (recv(sys[i].sock, &otw, sizeof (struct wire), 0) <
        sizeof (struct wire)) {
        fprintf(stderr, "Error reading from %s: %s\n", sys[i].s,
            strerror(errno));
        return -1;
#ifdef SHOW_STATISTICS
    } else if ((ntohs(otw.dport) == 1) && (ntohs(otw.sport) == 1) &&
        (otw.saddr == 1) && (otw.daddr == 1) &&
        (ntohl(otw.flowid) == FLOWID_CPU)) { /* CPU report */
        when = ntohl(otw.sec);
        DPRINTF(0, "%u.%06d:%s:C:%u:0\n",
            when, ntohl(otw.usec), sys[i].s, ntohs(otw.len));
        printf("%u.%06d:%s:C:%u:0\n",
            when, ntohl(otw.usec), sys[i].s, ntohs(otw.len));
#ifdef REPORT_MEM
    } else if ((ntohs(otw.dport) == 2) && (ntohs(otw.sport) == 2) &&
        (otw.saddr == 2) && (otw.daddr == 2) &&
        (ntohl(otw.flowid) == FLOWID_MEM)) { /* memory report */
        when = ntohl(otw.sec);
        printf("%u.%06d:%s:M:%u:0\n", when, ntohl(otw.usec),
            sys[i].s, (ntohs(otw.len) << 8) * 1024);
#endif
#endif
    } else {
        when = ntohl(otw.sec);
        ffr = findflow(ntohs(otw.dport)
#ifdef USE_MGEN
            , ntohl(otw.flowid)
#endif
            );
        if (ffr == 0) { /* No flow matched */
            printf("%lu.%06d:%s:X:%u:%c\n", when, ntohl(otw.usec),
                sys[i].s, ntohs(otw.len) * 8, otw.encapped ? '1' : '0');
        } else {
            for (k = 0; k < nflows; k++) {
                if (ffr & (1 << k)) {
                    printf("%lu.%06d:%s:%u:%u:%c\n",
                        when, ntohl(otw.usec), sys[i].s, flows[k].id,
                        ntohs(otw.len) * 8, otw.encapped ? '1' : '0');
#if 1 || defined(ENCAP_DBLCOUNT)
                    otw.encapped++;
#else
                    break; /* XXX stops double-counting, for better or worse */
#endif
                }
            }
        }
    }
#if 1
    if (last != otw.sec) {
        t = gmtime(&when);
        fprintf(stderr, "\r%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        last = otw.sec;
    }
#endif
    fflush(stdout);
    return 0;
}

int main(int argc, char *argv[]) {
    char buf[LINELEN];
    char *cfg = CONFFILE;
    int i;
    fd_set rfs, xfs;
    FILE *f;
    pthread_t ptconn;
    struct timeval tv;
    int shouldsig = 0;

    fputs("Traffic Flow Collector 3.0\nFortian Inc.\nwww.fortian.com\n\n",
        stderr);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-c")) {
            if (++i < argc) {
                cfg = argv[i];
            } else {
                fputs("You must specify a configuration file when using -c.\n",
                    stderr);
                return 1;
            }
        } else {
            fprintf(stderr, "Usage: %s [-c config]\n", argv[0]);
            return 2;
        }
    }

    FD_ZERO(&bigfs);
    FD_ZERO(&needconn);

    f = fopen(cfg, "r");
    if (f == NULL) {
        fprintf(stderr, "%s: couldn't open configuration file %s: %s\n",
            argv[0], cfg, strerror(errno));
        return 3;
    }
    while (fgets(buf, LINELEN, f) != NULL) {
        buf[LINELEN - 1] = 0;
        addconn(buf);
        if (feof(f)) break;
    }
    fclose(f);
    fprintf(stderr, "%s: configuration file loaded, beginning processing...\n",
        argv[0]);

    if (pthread_create(&ptconn, NULL, conner, argv[0]) < 0) {
        fprintf(stderr, "%s: couldn't create connections thread: %s",
            argv[0], strerror(errno));
        return errno;
    }
    pthread_detach(ptconn);
    pthread_cond_signal(&waitconn);

    for (;;) {
        pthread_mutex_lock(&connlock);
        memcpy(&rfs, &bigfs, sizeof (fd_set));
        memcpy(&xfs, &bigfs, sizeof (fd_set));
        pthread_mutex_unlock(&connlock);

        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fprintf(stderr, "recollect: examining up to FD %d...\n", lastsock);
        if (select(lastsock + 1, &rfs, NULL, &xfs, &tv) > 0) {
            for (i = 0; i < nsys; i++) {
                if (FD_ISSET(sys[i].sock, &xfs)) {
                    fprintf(stderr,
                        "\rrecollect: closing socket for %s due to exception\n",
                        sys[i].s);
                    if (!clobber(i)) {
                        shouldsig++;
                    }
                } else if (FD_ISSET(sys[i].sock, &rfs) && consume(i)) {
                    fprintf(stderr,
                        "\rrecollect: closing errored socket for %s\n",
                        sys[i].s);
                    if (!clobber(i)) {
                        shouldsig++;
                    }
                }
            }
            if (shouldsig) {
                pthread_cond_signal(&waitconn);
                shouldsig = 0;
            }
#if 0
        } else {
            puts("0:0:0");
            fflush(stdout);
#endif
        }
    }

    pthread_cancel(ptconn);
    for (i = 0; i < nsys; i++) {
        if (FD_ISSET(sys[i].sock, &bigfs)) {
            shutdown(sys[i].sock, SHUT_RDWR);
        }
        close(sys[i].sock);
    }
    free(sys);
    free(flows);

    return 0;
}
