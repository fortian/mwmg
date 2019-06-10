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
#include "linuxif.h"

#define SIN struct sockaddr_in
#define ETA struct ether_addr
/* Learn the MAC and IP of an interface.  Returns errno on failure. */
int manhandle(char *dsttmac, struct ether_addr *dstbmac, struct in_addr *dstip,
    int *dstidx, const char *ifname, short *flags) {
    char buf[BUFSIZ];
    struct ifconf ifc;
    struct ifreq *ifr;
    int sock;
    int nifs;
    int i;
    int rv = ENOENT;
    uint8_t *mac;

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
    for (i = 0; i < nifs; i++) {
        if (!strcmp(ifr[i].ifr_name, ifname)) {
            if (flags != NULL) {
                if (ioctl(sock, SIOCGIFFLAGS, &ifr[i]) < 0) {
                    perror("ioctl(SIOCGIFFLAGS)");
                    *flags = 0; /* It's unlikely that no flags could be set. */
                } else {
                    *flags = ifr[i].ifr_flags;
                }
            }
            if (dstip != NULL) {
                memcpy(dstip, &((SIN *)&ifr[i].ifr_addr)->sin_addr,
                    sizeof (struct in_addr));
            }

            if (dsttmac != NULL) {
                if (ioctl(sock, SIOCGIFHWADDR, &ifr[i]) < 0) {
                    rv = errno;
                    perror("ioctl(SIOCGIFHWADDR)");
                    close(sock);
                    return rv;
                }
                mac = ((struct ether_addr *)&(ifr[i].ifr_hwaddr.sa_data))->ether_addr_octet;
                snprintf(dsttmac, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }

            if (dstbmac != NULL) {
                mac = ((struct ether_addr *)&(ifr[i].ifr_hwaddr.sa_data))->ether_addr_octet;
                memcpy(dstbmac, mac, 6);
            }

            if (dstidx != NULL) {
                *dstidx = i;
            }

            rv = 0;
            break;
        }
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
