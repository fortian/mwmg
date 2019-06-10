#ifndef __FTFM_H
#define __FTFM_H

/* Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
Copyright (c) 2019, Fortian Inc.
All rights reserved.

See the file LICENSE which should have accompanied this software. */

#include <inttypes.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>

#define MACLEN 18
#define PCAPPER_PORT 7073
#define LISTENDEV "eth0"
#define SERVERDEV "ctrl0"
#define PROMISC_MODE 0
#define SNAP_LEN 96
#define TIMEOUT_MS 10
#define TIMEOUT_US 100000

#ifdef SHOW_STATISTICS
#define FLOWID_CPU 0xFFFFFFFF
#ifdef REPORT_MEM
#define FLOWID_MEM 0xFFFFFFFE
#endif
#endif

struct ip_pkt {
    struct iphdr h;
    union {     
        struct tcphdr th;
        struct udphdr uh;
        struct icmphdr ih;
    } p;    
    uint8_t raw[0];
} __attribute__ ((__packed__));
                    
struct packet {     
    struct ether_header eh;
    union {
        struct ether_arp arp;
        struct ip_pkt ip;
    } p;
} __attribute__ ((__packed__));

struct wire {
    uint32_t sec;
    uint32_t usec;
    uint32_t saddr;
    uint32_t daddr;
    uint16_t sport;
    uint16_t dport;
    uint16_t len;
#if 1 || defined(USE_MGEN) || defined(SHOW_STATISTICS)
    /* Breaks old on-the-wire-format, but no old clients should still exist. */
    uint32_t flowid;
#endif
    uint8_t proto;
    uint8_t encapped; /* was reserved */
} __attribute__ ((__packed__));

enum { MODE_SRC, MODE_DST, MODE_SPOOF };

struct flow {
    uint16_t id;
    uint16_t port[2];
    uint32_t mgenid[2];
};

#endif
