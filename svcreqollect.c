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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include <curl/curl.h>
#ifdef JSON_H_IN_JSON
#include <json/json.h>
#else
#include <json-c/json.h>
#endif
#include <stdint.h>

#define CONFFILE "collector.conf"
#define LINELEN 32

struct host_to_ip {
    char *host;
    char **ips;
    unsigned int ipn;
} *map = NULL;
unsigned int nhosts = 0;

struct portmap {
    uint16_t dport;
    unsigned long *accepted;
    unsigned long *rejected;
} *priorities = NULL;
size_t nprio;

char *jsontext = NULL;
size_t jsonlen = 0;

const int interval = 15; /* XXX make me configurable */

#define SVC_REQ_URI "http://127.0.0.1:8618/stats/net/servicerequests"
/* #define PRIORITY_REQ_URI "http://172.16.0.30/stats/conf/priority_table" */

/* #define NODE_INFO "/home/user/demo/casey/node_info.json" */
#define NODE_INFO "node_info.json"

#if 0
static void dump_prio_map(void) {
    size_t i;

    for (i = 0; i < nprio; i++) {
        fprintf(stderr, "%zu => %u\n", i, priorities[i].dport);
    }
}

static void dump_host_map(void) {
    unsigned int i, j;

    for (i = 0; i < nhosts; i++) {
        fprintf(stderr, "%u: host: %s, ipn: %u\n",
            i, (map[i].host == NULL) ? "(null)" : map[i].host, map[i].ipn);
        fputc('\t', stderr);
        for (j = 0; j < map[i].ipn; j++) {
            if (j) {
                fputs(", ", stderr);
            }
            fprintf(stderr, "%s",
                (map[i].ips[j] == NULL) ? "(null)" : map[i].ips[j]);
        }
        fputs("\n\n", stderr);
    }
}
#endif

static void addprio(char *line) {
    char *idx;
    struct portmap *h;

    idx = strchr(line, '\n');
    if (idx != NULL) {
        *idx = 0;
    }
    idx = strchr(line, '#');
    if (idx != NULL) {
        *idx = 0;
    }
    idx = strchr(line, '=');
    if (idx != NULL) {
        *idx++ = 0;
        h = realloc(priorities, sizeof (struct portmap) * (1 + nprio));
        if (h == NULL) {
            perror("addprio realloc");
        } else {
            priorities = h;
            if ((priorities[nprio].dport = atoi(idx))) {
                nprio++;
            }
        }
    } /* else, ignore, we get our numbers from somewhere else */
}

unsigned int find_host(const char *ip) {
    unsigned int i, j;

    for (i = 0; i < nhosts; i++) {
        for (j = 0; j < map[i].ipn; j++) {
            if (!strcmp(map[i].ips[j], ip)) {
                return i;
            }
        }
    }
    return UINT_MAX;
}

unsigned int find_port(uint16_t portnum) {
    size_t i;

    for (i = 0; i < nprio; i++) {
        if (priorities[i].dport == portnum) {
            return i;
        }
    }
    return UINT_MAX;
}

size_t curldata(char *ptr, size_t size, size_t nelem, void *data) {
    char *holder;
    char **jt;
    size_t rv = 0;

    jt = (char **)data;
    holder = realloc(*jt, jsonlen + size * nelem + 1);
    if (holder == NULL) {
        perror("realloc");
    } else {
        *jt = holder;
        memcpy(&holder[jsonlen], ptr, size * nelem);
        jsonlen += size * nelem;
        holder[jsonlen] = 0;
        rv = size * nelem;
    }
    return rv;
}

void add_ip_to_host(const char *hostname, const char *orig_ip) {
    unsigned int i, j;
    char **h;
    char *idx;
    struct host_to_ip *hh;

    for (i = 0; i < nhosts; i++) {
        if (!strcmp(map[i].host, hostname)) {
            break;
        }
    }

    if (i >= nhosts) {
        hh = realloc(map, sizeof (struct host_to_ip) * (nhosts + 1));
        if (hh == NULL) {
            perror("add_ip_to_host realloc 1");
            return;
        }
        map = hh;
        map[nhosts].host = strdup(hostname);
        map[nhosts].ipn = 0;
        map[nhosts].ips = NULL;
        nhosts++;
    }

    j = map[i].ipn;
    h = realloc(map[i].ips, (1 + j) * sizeof (char *));
    if (h == NULL) {
        perror("add_ip_to_host realloc 2");
        return;
    }
    map[i].ips = h;
    // dump_host_map();
    map[i].ips[j] = strdup(orig_ip);
    idx = strchr(map[i].ips[j], '/');
    if (idx != NULL) {
        *idx = 0;
    }
    map[i].ipn++;
    // dump_host_map();
}

json_object *json_get_object(json_object *j, const char *k) {
    json_object *rv;

#ifndef THIS_FUNCTION_IS_DEPRECATED
    rv = json_object_object_get(j, k);
#else
    if (!json_object_object_get_ex(j, k, &rv)) {
        rv = NULL;
    }
#endif
    return rv;
}

uint16_t json_get_uint16(json_object *j, const char *k) {
    json_object *h;
    int64_t jrv;
    uint16_t rv;

    h = json_get_object(j, k);
    if (h == NULL) {
        rv = 0;
    } else {
        jrv = json_object_get_int(h);
        if ((jrv < 0) || (jrv > UINT16_MAX)) {
            rv = 0;
        } else {
            rv = (uint16_t)(jrv & 0xFFFF);
        }
    }
    return rv;
}

const char *json_get_string(json_object *j, const char *k) {
    json_object *h;
    const char *rv;

    h = json_get_object(j, k);
    if (h == NULL) {
        rv = NULL;
    } else {
        rv = json_object_get_string(h);
    }
    return rv;
}

int main(int argc, char *argv[]) {
    unsigned long senders;
    array_list *arr, *paths;
    json_object *json, *h;
    int i, j, len;
    struct tm *t;
    unsigned long acct, rejt, aa, rr;
    FILE *f = NULL;
    char *uri = SVC_REQ_URI;
    char *cfg = CONFFILE;
    char *nif = NODE_INFO;
    CURL *rest = NULL;
    int rv = 0;
    unsigned int live = 1;
    unsigned int badflag = 0;
    char *fn[2] = { "servicerequests-1.json", "servicerequests-2.json" };
    char buf[LINELEN];

    fputs("Service Request Collector 1.0\nFortian Inc.\nwww.fortian.com\n\n",
        stderr);

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'c') {
                if (argv[i][2]) {
                    cfg = &argv[i][2];
                } else if (argc > ++i) {
                    cfg = argv[i];
                } else {
                    fprintf(stderr, "%s: -c requires a configuration file\n",
                        argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'j') {
                if (argv[i][2]) {
                    nif = &argv[i][2];
                } else if (argc > ++i) {
                    nif = argv[i];
                } else {
                    fprintf(stderr, "%s: -j requires a configuration file\n",
                        argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'u') {
                if (argv[i][2]) {
                    uri = &argv[i][2];
                } else if (argc > ++i) {
                    uri = argv[i];
                } else {
                    fprintf(stderr, "%s: -u requires a URI\n", argv[0]);
                    badflag = __LINE__;
                }
            } else if (argv[i][1] == 'x') {
                live = 0;
            } else {
                badflag = __LINE__;
            }
        } else {
            badflag = __LINE__;
        }
    }
    if (badflag) {
        fprintf(stderr,
            "Usage: %s [-c config] [-j node-info-json] [-u uri] [-x]\n",
            argv[0]);
        return 1;
    }

    priorities = calloc(1, sizeof (struct portmap));
    nprio = 1;

    f = fopen(cfg, "r");
    if (f == NULL) {
        fprintf(stderr, "Could not open configuration file %s: %s\n", cfg,
            strerror(errno));
        return 3;
    }
    while (fgets(buf, LINELEN, f) != NULL) {
        buf[LINELEN - 1] = 0;
        addprio(buf);
        if (feof(f)) {
            break;
        }
    }
    fclose(f);
    fputs("1st configuration file loaded...\n", stderr);

    json = json_object_from_file(nif);
    rv = (int)(long)json;
    if (rv < 0) {
        json = NULL;
        rv = -rv;
        goto abend;
    }

    json_object_object_foreach(json, topkey, topval) {
        if (strstr(topkey, "Host") != NULL) { /* ignore non-hosts */
            json_object_object_foreach(topval, midkey, midval) {
                (void)midkey;
                const char *word = json_get_string(midval, "address");
                if (word != NULL) {
                    add_ip_to_host(topkey, word);
                }
            }
        }
    }
    json_object_put(json);
    fputs("2nd configuration file loaded...\n", stderr);

    /* dump_prio_map(); */

    for (i = 0; i < nprio; i++) {
        /* clang insists that nhosts is always null, and I'm sure it's wrong
        because it gets modified in add_ip_to_host.  The challenge here is
        that excessive optimization might agree with clang and optimize this
        out, causing a segfault. */
        priorities[i].accepted = malloc(nhosts * sizeof (unsigned long));
        priorities[i].rejected = malloc(nhosts * sizeof (unsigned long));
    }

    if ((rv = curl_global_init(CURL_GLOBAL_ALL))) {
        fprintf(stderr, "%s: cURL global initialization failed!\n", argv[0]);
        goto abend;
    }
    if ((rest = curl_easy_init()) == NULL) {
        fprintf(stderr, "%s: cURL easy initialization failed!\n", argv[0]);
        goto abend;
    }

    if (live) {
        /* curl_easy_setopt(rest, CURLOPT_VERBOSE, 1L); */
        curl_easy_setopt(rest, CURLOPT_URL, uri);
        curl_easy_setopt(rest, CURLOPT_WRITEFUNCTION, curldata);
        curl_easy_setopt(rest, CURLOPT_WRITEDATA, &jsontext);
    }

    for (;;) {
        json_object *zero, *path;
        struct timeval tv1, tv2, tv3;
        int srcport, tgthost, tgtport;

        if (gettimeofday(&tv1, NULL) < 0) {
            perror("gettimeofday 1");
            rv = 5;
            break;
        }

        for (i = 0; i < nprio; i++) {
            memset(priorities[i].accepted, 0, nhosts * sizeof (unsigned long));
            memset(priorities[i].rejected, 0, nhosts * sizeof (unsigned long));
        }

        if (live) {
            if ((rv = curl_easy_perform(rest))) {
                fprintf(stderr, "%s: cURL failed: %d: %s\n",
                    argv[0], rv, curl_easy_strerror(rv));
                rv = 18;
                break;
            }
            if (jsontext == NULL) {
                fprintf(stderr, "%s: cURL didn't load JSON\n", argv[0]);
                rv = 19;
                break;
            }

            json = json_tokener_parse(jsontext);
        } else {
            if (badflag++ %2) { /* reuse the zeroed-out badflag */
                json = json_object_from_file(fn[0]);
            } else {
                json = json_object_from_file(fn[1]);
            }
        }
        rv = (int)(long)json;
        if (rv < 0) {
            json = NULL;
            rv = -rv;
            fprintf(stderr, "%s: JSON service request parser failed!\n",
                argv[0]);
            goto abend;
        }
        if (jsontext != NULL) {
            free(jsontext);
            jsontext = NULL;
            jsonlen = 0;
        }

        arr = json_object_get_array(json);
        if (arr == NULL) {
            fprintf(stderr, "%s: JSON response doesn't contain an array!\n",
                argv[0]);
            rv = 6;
            goto abend;
        }
        len = array_list_length(arr);
        aa = rr = senders = 0L;
        for (i = 0; i < len; i++) {
            const char *roleval, *srcip, *destip;
            h = array_list_get_idx(arr, i);
            if (h == NULL) {
                fprintf(stderr, "%s: Couldn't access element %d of response!\n",
                    argv[0], i);
                rv = 7;
                goto abend;
            } else if ((zero = json_get_object(h, "#0")) == NULL) {
                fprintf(stderr, "%s: [%d].#0 doesn't exist!\n", argv[0], i);
                rv = 9;
                goto abend;
            } else if ((srcip = json_get_string(zero, "src_ip")) == NULL) {
                fprintf(stderr, "%s: [%d].#0 doesn't have key 'src_ip'!\n",
                    argv[0], i);
                rv = 15;
                goto abend;
            } else if ((destip = json_get_string(zero, "dst_ip")) == NULL) {
                fprintf(stderr, "%s: [%d].#0 doesn't have key 'dst_ip'!\n",
                    argv[0], i);
                rv = 20;
                goto abend;
            } else if (((tgthost = find_host(srcip)) == UINT_MAX) &&
                ((tgthost = find_host(destip)) == UINT_MAX)) {
                fprintf(stderr, "%s: [%d].#0.src_ip %s isn't in host map!\n",
                    argv[0], i, srcip);
                rv = 12;
                goto abend;
            } else if (!(srcport = json_get_uint16(zero, "destination-port"))) {
                fprintf(stderr,
                    "%s: [%d].#0 doesn't have key 'destination-port'!\n",
                    argv[0], i);
                rv = 14;
                goto abend;
#if 0
            } else if ((tgtport = find_port(srcport)) == UINT_MAX) {
                fprintf(stderr,
                    "%s: [%d].#0.destination-port %d isn't in map!\n",
                    argv[0], i, srcport);
                rv = 17;
                goto abend;
#endif
            } else if ((roleval = json_get_string(zero, "roles")) == NULL) {
                fprintf(stderr, "%s: [%d].#0.roles isn't defined!\n", argv[0], i);
                rv = 10;
                goto abend;
            } else if (!strcasecmp(roleval, "SENDER")) {
                senders++;
                if ((tgtport = find_port(srcport)) == UINT_MAX) {
                    fprintf(stderr,
                        "\r%s: Couldn't find port for %u, continuing...\n",
                        argv[0], srcport);
                    tgtport = 0;
                }
                if ((path = json_get_object(h, "path")) == NULL) {
                    fprintf(stderr,
                        "\r%s: [%d].#0 doesn't have key 'path', continuing....\n",
                        argv[0], i);
                    priorities[tgtport].rejected[tgthost]++;
                    rr++;
                } else if ((paths = json_object_get_array(path)) == NULL) {
                    fprintf(stderr,
                        "\r%s: [%d].#0.path isn't an array, continuing...\n",
                        argv[0], i);
                    priorities[tgtport].rejected[tgthost]++;
                    rr++;
                } else if (array_list_length(paths) > 0) {
                    priorities[tgtport].accepted[tgthost]++;
                    aa++;
                } else {
                    priorities[tgtport].rejected[tgthost]++;
                    rr++;
                }
            } else {
                /* ignore non-senders for now */
            }
        }

        acct = rejt = 0;
        for (j = 0; j < nhosts; j++) {
            printf("%ld.%06ld:%u:X:%lu:0\n",
                tv1.tv_sec, tv1.tv_usec, j + 1, priorities[0].accepted[j]);
            acct += priorities[0].accepted[j];
            printf("%ld.%06ld:%u:X:%lu:1\n",
                tv1.tv_sec, tv1.tv_usec, j + 1, priorities[0].rejected[j]);
            rejt += priorities[0].rejected[j];
        }
        fprintf(stderr, "\r           Unknown: %lu admitted, %lu non-admitted\n",
            acct, rejt);

        for (i = 1; i < nprio; i++) {
            acct = rejt = 0;
            for (j = 0; j < nhosts; j++) {
                printf("%ld.%06ld:%u:%u:%lu:0\n",
                    tv1.tv_sec, tv1.tv_usec, j + 1, i,
                    priorities[0].accepted[j]);
                acct += priorities[0].accepted[j];
                printf("%ld.%06ld:%u:%u:%lu:1\n",
                    tv1.tv_sec, tv1.tv_usec, j + 1, i,
                    priorities[0].rejected[j]);
                rejt += priorities[0].rejected[j];
            }
            fprintf(stderr, "\r   Priority %02u: %lu accepted, %lu rejected\n",
                i, acct, rejt);
        }
        t = gmtime(&tv2.tv_sec);
        fprintf(stderr, "\r%02d:%02d:%02d TOTAL: %lu accepted, %lu rejected\n",
            t->tm_hour, t->tm_min, t->tm_sec, aa, rr);

        json_object_put(json);
        json = NULL;
        tv1.tv_sec += interval;
        if (gettimeofday(&tv2, NULL) < 0) {
            perror("gettimeofday 2");
            rv = 13;
            goto abend;
        }
        timersub(&tv1, &tv2, &tv3);
        /* Are both non-negative and is one positive? */
        while ((tv3.tv_sec >= 0) && (tv3.tv_usec >= 0) &&
            (tv3.tv_sec && tv3.tv_usec)) {
            if (usleep(tv3.tv_usec) < 0) {
                if (errno == EINTR) {
                    if (gettimeofday(&tv2, NULL) < 0) {
                        /* should be impossible by now */
                        perror("gettimeofday 3");
                        rv = 16;
                        goto abend;
                    }
                    timersub(&tv1, &tv2, &tv3);
                } else {
                    break;
                }
            }
        }
    }

abend:
    if (map != NULL) {
        for (i = 0; i < nhosts; i++) {
            free(map[i].host);
            for (j = 0; j < map[i].ipn; j++) {
                free(map[i].ips[j]);
            }
            free(map[i].ips);
        }
        free(map);
    }
    if (priorities != NULL) {
        for (i = 0; i < nprio; i++) {
            free(priorities[i].accepted);
            free(priorities[i].rejected);
        }
        free(priorities);
    }
    if (json != NULL) {
        json_object_put(json);
    }
    curl_easy_cleanup(rest);
    curl_global_cleanup();
    return rv;
}
