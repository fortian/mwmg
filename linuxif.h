#ifndef __LINUXIF_H
#define __LINUXIF_H

/* Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
Copyright (c) 2019, Fortian Inc.
All rights reserved.

See the file LICENSE which should have accompanied this software. */

#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#define IFCLEN 512

/* Learn the MAC and IP of an interface.  Returns errno on failure. */
int manhandle(char *dsttmac, struct ether_addr *dstbmac, struct in_addr *dstip,
    int *dstidx, const char *ifname, short *flags);
int gettmac(char *dst, const char *ifname);
int getbmac(struct ether_addr *dst, const char *ifname);
int getip(struct in_addr *dst, const char *ifname, short *flags);
int getidx(int *dst, const char *ifname);
#endif
