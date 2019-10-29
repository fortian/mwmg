/* Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
Copyright (c) 2019, Fortian Inc.
All rights reserved.

See the file LICENSE which should have accompanied this software. */

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <errno.h>
#include <ctype.h>
#include "linuxif.h"

#define SIN struct sockaddr_in
#define ETA struct ether_addr
/* Learn the MAC and IP of an interface.  Returns errno on failure. */
int manhandle(char *dsttmac, ETA *dstbmac, struct in_addr *dstip,
    int *dstidx, const char *ifname, short *flags) {
    char buf[BUFSIZ];
    struct ifconf ifc;
    struct ifreq *ifr;
    int sock;
    int nifs;
    int i;
    int rv = ENOENT;
    uint8_t *mac;
    int ethidx = -1;
    /* I want bitfields but the compiler is yelling at me, so wevs. */
    int dsttmacset = 0;
    int flagset = 0;
    int dstipset = 0;
    int dstbmacset = 0;
    int dstidxset = 0;

    /* Get a socket handle. */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        rv = errno;
        return rv;
    }

    /* Query available interfaces. */
    ifc.ifc_len = sizeof (buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) < 0) {
        rv = errno;
        perror("ioctl(SIOCGIFCONF)");
        close(sock);
        return rv;
    }

    /* Iterate through the list of interfaces. */
    ifr = ifc.ifc_req;
    nifs = ifc.ifc_len / sizeof (struct ifreq);
    for (i = 0; (i < nifs) && (ethidx < 0); i++) {
        if (!strcmp(ifr[i].ifr_name, ifname)) {
            ethidx = i;
        }
    }

    if (ethidx >= 0) {
        if (dsttmac != NULL) {
            if (ioctl(sock, SIOCGIFHWADDR, &ifr[ethidx]) < 0) {
                rv = errno;
                fprintf(stderr, "ioctl(%s, SIOCGIFHWADDR): %s\n",
                    ifname, strerror(errno));
                close(sock);
                return rv;
            }
#pragma GCC diagnostic push
            /* The lines with struct ether_addr * annoy the compiler. */
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
            mac = ((ETA *)&(ifr[ethidx].ifr_hwaddr.sa_data))->ether_addr_octet;

#if 0
            if ((mac[0] != 2) || (mac[1] != 2) || (mac[2] != 0)) {
#endif
                snprintf(dsttmac, MACLEN, "%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                dsttmac[MACLEN] = 0;
                dsttmacset = 1;
#if 0
            }
#endif
        }

        if (flags != NULL) {
            if (ioctl(sock, SIOCGIFFLAGS, &ifr[ethidx]) < 0) {
                fprintf(stderr, "ioctl(%s, SIOCGIFFLAGS): %s\n",
                    ifname, strerror(errno));
                *flags = 0; /* It's unlikely that no flags could be set. */
            } else {
                *flags = ifr[ethidx].ifr_flags;
                flagset = 1;
            }
        }

        if (dstip != NULL) {
            memcpy(dstip, &((SIN *)&ifr[ethidx].ifr_addr)->sin_addr,
                sizeof (struct in_addr));
            dstipset = 1;
        }

        if (dstbmac != NULL) {
            mac = ((ETA *)&(ifr[ethidx].ifr_hwaddr.sa_data))->ether_addr_octet;
            memcpy(dstbmac, mac, 6);
            dstbmacset = 1;
        }
#pragma GCC diagnostic pop

        if (dstidx != NULL) {
            *dstidx = ethidx;
            dstidxset = 1;
        }

        rv = -!(dsttmacset || flagset || dstipset || dstbmacset || dstidxset);
    }

    close(sock);
    return rv;
}
#undef SIN
#undef ETA

int gettmac(char *dst, const char *ifname) {
    return manhandle(dst, NULL, NULL, NULL, ifname, NULL);
}

int getbmac(struct ether_addr *dst, const char *ifname) {
    return manhandle(NULL, dst, NULL, NULL, ifname, NULL);
}

int getip(struct in_addr *dst, const char *ifname, short *flags) {
    return manhandle(NULL, NULL, dst, NULL, ifname, flags);
}

int getidx(int *dst, const char *ifname) {
    return manhandle(NULL, NULL, NULL, dst, ifname, NULL);
}
