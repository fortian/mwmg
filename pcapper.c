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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pcap.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <inttypes.h>
#include "ftfm.h"
#include "linuxif.h"

#define TAGIP "SERVERIP="
#define TAGPORT "SERVERPORT="
#define SRCFILTER "ether src %s"
#define DSTFILTER "ether src not %s"
#define SPOFILTER "src %s and ether src not %s"
#define FILTERLEN 64
#define LOGFILE "pcapper.log"

fd_set bigfs = { { 0 } };
pthread_mutex_t biglock = PTHREAD_MUTEX_INITIALIZER;

enum { S_USER, S_NICE, S_SYS, S_IDLE, S_IO, S_IRQ, S_SOFTIRQ, S_TOT, S_MAX };
enum { M_TOT, M_FREE, M_BUF, M_CACHE, M_SWAPCACHE, M_MAX };

char *logfn = LOGFILE;

void *acceptor(void *sockp) {
    fd_set rfs, xfs;
    int sock = (long)sockp;
    int i;
    int conn;

    for (;;) {
        FD_ZERO(&rfs);
        FD_ZERO(&xfs);
        FD_SET(sock, &rfs);
        FD_SET(sock, &xfs);
        i = select(sock + 1, &rfs, NULL, &xfs, NULL);
        if ((i < 0) && (errno != EINTR)) {
            perror("select");
        } else if (i > 0) {
            if (FD_ISSET(sock, &xfs)) {
                perror("select");
            } else if ((conn = accept(sock, NULL, NULL)) < 0) {
                perror("accept");
            } else {
                pthread_mutex_lock(&biglock);
                FD_SET(conn, &bigfs);
                pthread_mutex_unlock(&biglock);
            }
        }
    }
    return NULL;
}


void dumppacket(const unsigned char *pkt, size_t len) {
    size_t i, j;

    for (i = 0; i < len; i += 20) {
        for (j = 0; (j < 20) && (i+j < len); j++) {
            printf("%2.2X ", (unsigned)(pkt[i+j] & 0xFF));
        }
        while (j < 20) {
            printf("   ");
            j++;
        }
        for (j = 0; (j < 20) && (i+j < len); j++) {
            if (isprint((unsigned)pkt[i+j])) putchar(pkt[i+j]);
            else putchar('.');
        }
        putchar('\n');
    }
    fflush(stdout);
}

void do_udp(struct udphdr *p) {
    printf("Source port: %u\n", p->uh_sport);
    printf("Destination port: %u\n", p->uh_dport);
    printf("Length: %u\n", p->uh_ulen);
}

void do_tcp(struct tcphdr *p) {
    printf("Source port: %u\n", p->th_sport);
    printf("Destination port: %u\n", p->th_dport);
    printf("Sequence number: %u\n", p->th_seq);
    printf("Acknowledgement number: %u\n", p->th_ack);
}

void do_icmp(struct icmphdr *p) {
    printf("Type: ");
    switch (p->type) {
        case ICMP_ECHOREPLY: puts("Echo Reply"); break;
        case ICMP_DEST_UNREACH: puts("Destination Unreachable"); break;
        case ICMP_SOURCE_QUENCH: puts("Source Quench"); break;
        case ICMP_REDIRECT: puts("Redirect"); break;
        case ICMP_ECHO: puts( "Echo Request"); break;
        case ICMP_TIME_EXCEEDED: puts("Time Exceeded"); break;
        case ICMP_PARAMETERPROB: puts("Parameter Problem"); break;
        case ICMP_TIMESTAMP: puts("Timestamp Request"); break;
        case ICMP_INFO_REQUEST: puts("Information Request"); break;
        case ICMP_INFO_REPLY: puts("Information Reply"); break;
        case ICMP_ADDRESS: puts("Address Mask Request"); break;
        case ICMP_ADDRESSREPLY: puts("Address Mask Reply"); break;
        case ICMP_TIMESTAMPREPLY: puts("Timestamp Reply"); break;
        default: printf("Unknown ICMP type %u\n", p->type);
    }
    printf("Code: ");
    switch (p->type) {
        case ICMP_DEST_UNREACH:
            switch (p->code) {
                case ICMP_NET_UNREACH: puts("Network Unreachable"); break;
                case ICMP_HOST_UNREACH: puts("Host Unreachable"); break;
                case ICMP_PROT_UNREACH: puts("Protocol Unreachable"); break;
                case ICMP_PORT_UNREACH: puts("Port Unreachable"); break;
                case ICMP_FRAG_NEEDED: puts("Fragmentation Needed"); break;
                case ICMP_SR_FAILED: puts("Source Route Failed"); break;
                case ICMP_NET_UNKNOWN: puts("Network Unknown"); break;
                case ICMP_HOST_UNKNOWN: puts("Host Unknown"); break;
                case ICMP_HOST_ISOLATED: puts("Host Isolated"); break;
                case ICMP_NET_ANO: puts("Network ANO"); break;
                case ICMP_HOST_ANO: puts("Host ANO"); break;
                case ICMP_NET_UNR_TOS: puts("Network Unreachable/TOS"); break;
                case ICMP_HOST_UNR_TOS: puts("Host Unreachable/TOS"); break;
                case ICMP_PKT_FILTERED: puts("Packet Filtered"); break;
                case ICMP_PREC_VIOLATION: puts("Precedence Violation"); break;
                case ICMP_PREC_CUTOFF: puts("Precedence Cut Off"); break;
                default: printf("Unknown Unreachable code %d\n", p->code);
            }
            break;
        case ICMP_REDIRECT:
            switch (p->code) {
                case ICMP_REDIR_NET: puts("Network Redirect"); break;
                case ICMP_REDIR_HOST: puts("Host Redirect"); break;
                case ICMP_REDIR_NETTOS: puts("Network Redirect/TOS"); break;
                case ICMP_REDIR_HOSTTOS: puts("Host Redirect/TOS"); break;
                default: printf("Unknown Redirect code %d\n", p->code);
            }
            break;
        case ICMP_TIME_EXCEEDED:
            switch (p->code) {
                case ICMP_EXC_TTL: puts("TTL Count Exceeded"); break;
                case ICMP_EXC_FRAGTIME: puts("Frag Reasm Time Exceeded"); break;
                default: printf("Unknown Time Exceeded code %d\n", p->code);
            }
            break;
        default: printf("Unknown code %d\n", p->code);
    }
}

void doip(struct ip_pkt *p) {
    struct in_addr hog;

    /* dumppacket(p, 32); */
    printf("Version: %d\n", p->h.version);
    printf("Header Length: %d\n", p->h.ihl);
    printf("TOS: %x\n", p->h.tos);
    printf("IP Packet Length: %d (%x)\n",
        ntohs(p->h.tot_len), ntohs(p->h.tot_len));
    printf("Packet ID: %x\n", p->h.id);
    printf("Fragment Offset: %d (%x)\n", p->h.frag_off, p->h.frag_off);
    printf("Inverted Fragment Offset: %d (%x)\n",
       ntohs(p->h.frag_off), ntohs(p->h.frag_off));
    printf("TTL: %u (%x)\n", p->h.ttl, p->h.ttl);
    hog.s_addr = p->h.saddr;
    printf("Source address: %s\n", inet_ntoa(hog));
    hog.s_addr = p->h.daddr;
    printf("Destination address: %s\n", inet_ntoa(hog));
    printf("Protocol: ");
    switch (p->h.protocol) {
        case IPPROTO_ICMP: puts("ICMP"); do_icmp(&p->p.ih); break;
        case IPPROTO_TCP: puts("TCP"); do_tcp(&p->p.th); break;
        case IPPROTO_UDP: puts("UDP"); do_udp(&p->p.uh); break;
        default: printf("Unknown protocol %u (%x)\n", p->h.protocol,
            p->h.protocol);
    }
    putchar('\n');
}

void doarp(struct ether_arp *p) {
    printf("Opcode: ");
    switch (ntohs(p->arp_op)) {
        case ARPOP_REQUEST: puts("ARP Request"); break;
        case ARPOP_REPLY: puts("ARP Reply"); break;
        case ARPOP_RREQUEST: puts("RARP Request"); break;
        case ARPOP_RREPLY: puts("RARP Reply"); break;
        case ARPOP_InREQUEST: puts("InARP Request"); break;
        case ARPOP_InREPLY: puts("InARP Reply"); break;
        case ARPOP_NAK: puts("ATM ARP NAK"); break;
        default: puts("Unknown ARP opcode");
    }
    if (p->arp_hrd != htons(ARPHRD_ETHER)) {
        printf("Not an Ethernet ARP, giving up - %x\n", ntohs(p->arp_hrd));
    } else {
        printf("Sender: %u.%u.%u.%u (%x:%x:%x:%x:%x:%x)\n",
            p->arp_spa[0], p->arp_spa[1], p->arp_spa[2], p->arp_spa[3],
            p->arp_sha[0], p->arp_sha[1], p->arp_sha[2], p->arp_sha[3],
            p->arp_sha[4], p->arp_sha[5]);
        printf("Destination: %d.%d.%d.%d (%x:%x:%x:%x:%x:%x)\n",
            p->arp_tpa[0], p->arp_tpa[1], p->arp_tpa[2], p->arp_tpa[3],
            p->arp_tha[0], p->arp_tha[1], p->arp_tha[2], p->arp_tha[3],
            p->arp_tha[4], p->arp_tha[5]);
    }
}

#if 0
void dumppkt(const u_char *pkt) {
    struct packet *p = (struct packet *)pkt;
    int i;

    /* dumppacket(pkt, 32); */
    printf("Source MAC address: ");
    for (i = 0; i < ETH_ALEN; i++) {
        if (i) putchar(':');
        printf("%0x", p->eh.ether_shost[i]);
    }
    printf("\nDestination MAC address: ");
    for (i = 0; i < ETH_ALEN; i++) {
        if (i) putchar(':');
        printf("%0x", p->eh.ether_dhost[i]);
    }
    printf("\nPacket type: %x ", ntohs(p->eh.ether_type));
    switch (ntohs(p->eh.ether_type)) {
        case ETHERTYPE_PUP: puts("PUP"); break;
        case ETHERTYPE_ARP: puts("ARP"); doarp(&p->p.arp); break;
        case ETHERTYPE_REVARP: puts("Reverse ARP"); doarp(&p->p.arp); break;
        case ETHERTYPE_IP: puts("IP"); doip(&p->p.ip); break;
        default: puts("Unknown type");
    }
}
#endif

void logpkt(struct wire *otw
#ifdef ENCAPS
    , struct wire *encap
#endif
    ) {
    FILE *f;
    struct in_addr ina;

    f = fopen(logfn, "a");
    if (f == NULL) {
        fprintf(stderr, "Could not log data to %s: %s\n",
            logfn, strerror(errno));
    } else {
        ina.s_addr = otw->saddr;
        fprintf(f, "%d.%06d:%s:%u:", ntohl(otw->sec), ntohl(otw->usec),
            inet_ntoa(ina), ntohs(otw->sport));
        ina.s_addr = otw->daddr;
        fprintf(f, "%s:%u:%u:"
#if 1 || defined(USE_MGEN) || defined(SHOW_STATISTICS)
           "%u:"
#endif
           "%u", inet_ntoa(ina), ntohs(otw->dport), ntohs(otw->len),
#if 1 || defined(USE_MGEN) || defined(SHOW_STATISTICS)
            ntohl(otw->flowid),
#endif
            otw->proto);
        /* N.B.: Logfile format changed 2015 08 07 to include MGEN flow ID */
#ifdef ENCAPS
        if ((encap != NULL) && encap->encapped) {
            ina.s_addr = encap->saddr;
            fprintf(f, ":%s:%u:", inet_ntoa(ina), ntohs(encap->sport));
            ina.s_addr = encap->daddr;
            fprintf(f, "%s:%u:%u:%u", inet_ntoa(ina), ntohs(encap->dport),
                ntohs(encap->len), otw->proto);
        }
#endif
        fputs("\n", f);
        /* fflush(f); */
        fclose(f);
        /* sync(); */
    }
}

void blastpacket(fd_set *f, const u_char *pkt, struct pcap_pkthdr *head) {
    int i;
    struct wire otw = { 0 };
#ifdef ENCAPS
    struct wire encap = { 0 };
#endif
    struct in_addr hog;
    char sip[16], dip[16];
    struct ip_pkt *p;
    uint16_t et;
    unsigned int headlen = 0;

    et = ntohs(((struct packet *)pkt)->eh.ether_type);

    if (et != ETHERTYPE_IP) {
        return;
    } else {
        p = (struct ip_pkt *)(&((struct packet *)pkt)->p.ip);
        if (ntohs(p->h.frag_off) & IP_OFFMASK) {
            /* This is bad, but since this is stateless, this packet is
            uncountable unless/until I do IP packet ID matching. */
            return;
        }
        if (p->h.version != 4) {
            return;
        }
        headlen = p->h.ihl * 4;
        if ((p->h.protocol == IPPROTO_TCP) || (p->h.protocol == IPPROTO_UDP)) {
#if 0
            prettyprint(pkt, head->caplen);
#endif
            otw.sec = htonl(head->ts.tv_sec);
            otw.usec = htonl(head->ts.tv_usec);
            otw.saddr = p->h.saddr;
            otw.daddr = p->h.daddr;
            /* same place in TCP and UDP */
            memcpy(&otw.sport, &p->p.th.th_sport + (headlen - 20), 2);
            memcpy(&otw.dport, &p->p.th.th_dport + (headlen - 20), 2);
            if (!otw.dport) {
                hog.s_addr = p->h.saddr;
                strcpy(sip, inet_ntoa(hog));
                sip[15] = 0;
                hog.s_addr = p->h.daddr;
                strcpy(dip, inet_ntoa(hog));
                dip[15] = 0;
            }
#if 1 || defined(USE_MGEN)
            otw.flowid = 0;
            if (head->caplen >= (0x42 + headlen - 20 + 4)) {
                if ((!memcmp(&pkt[0x3e + headlen - 20], &otw.dport, 2)) &&
                    (pkt[0x40 + headlen - 20] == 1) &&
                    (pkt[0x41 + headlen - 20] == 4) &&
                    (!memcmp(&pkt[0x42 + headlen - 20], &otw.daddr, 4))) {
                    memcpy(&otw.flowid, &pkt[0x2e + headlen - 20], 4);
                } else {
                    /* Can't find flowID, leave as 0. */
                }
            }
#endif
            otw.len = p->h.tot_len;
            otw.proto = p->h.protocol;
#ifdef ENCAPS
            /* This is legacy.  ENCAPS should be disabled unless you use BMF. */
            if ((otw.proto == IPPROTO_UDP) && (otw.dport == ntohs(50698))) {
                /* it has a header (we already checked if it was a fragment) */
                if (ntohs(otw.len) < 64) {
                    fprintf(stderr, "WARNING: not logging encapsulation in %u "
                        "byte packet.\n", ntohs(otw.len));
                } else {
                    encap.sec = htonl(head->ts.tv_sec); /* a good guess */
                    encap.usec = htonl(head->ts.tv_usec);
                    /* XXX should check BMF version - v1 is 8 bytes and data
                       starts at 0x32, while CenGen experimental is 12 bytes and
                       data starts at 0x36 */
                    p = (struct ip_pkt *)&pkt[0x36 + (headlen - 20)];
                    /* XXX should check IP version */
                    headlen = p->h.ihl * 4;
                    encap.saddr = p->h.saddr;
                    encap.daddr = p->h.daddr;
                    memcpy(&encap.sport, &p->p.th.th_sport + (headlen - 20), 2);
                    memcpy(&encap.dport, &p->p.th.th_dport + (headlen - 20), 2);
                    encap.len = p->h.tot_len;
                    encap.proto = p->h.protocol;
                    encap.encapped = 1;
                }
            }
#endif
#ifdef ENCAPS
            logpkt(&otw , &encap);
#else
            logpkt(&otw);
#endif
            pthread_mutex_lock(&biglock);
            /* don't report relays yet */
            for (i = 0; i < FD_SETSIZE; i++) if (FD_ISSET(i, f)) {
                if (send(i, &otw, sizeof (otw), MSG_NOSIGNAL) < 0) {
                    perror("send error");
                    FD_CLR(i, f);
                    shutdown(i, SHUT_RDWR);
                    close(i);
#ifdef ENCAPS
                } else if (encap.encapped &&
                    (send(i, &encap, sizeof (encap), MSG_NOSIGNAL) < 0)) {
                    perror("send encapped error");
                    FD_CLR(i, f);
                    shutdown(i, SHUT_RDWR);
                    close(i);
#endif
                }
            }
            pthread_mutex_unlock(&biglock);
        } else {
            /* Not IP, ignore. */
        }
    }
}

#ifdef SHOW_STATISTICS
uint64_t readmem(void) {
    FILE *f;
    char buf[BUFSIZ];
    char *idx;
    char *hold;
    uint64_t free = 0;
    uint64_t rv = 0;

    f = fopen("/proc/meminfo", "r");
    if (f == NULL) {
        perror("fopen");
    } else do {
        fgets(buf, BUFSIZ, f);
        buf[BUFSIZ - 1] = 0;
        if (!strncmp(buf, "MemTotal:", 9)) {
            idx = &buf[9];
            while (*idx == ' ') idx++;
            rv = strtoull(idx, NULL, 10);
            if (!rv) {
                fputs("Malformed total line in /proc/meminfo, giving up.\n",
                    stderr);
                break;
            }
        } else if (!(strncmp(buf, "MemFree:", 8) &&
            strncmp(buf, "Buffers:", 8) && strncmp(buf, "Cached:", 7) &&
            strncmp(buf, "SwapCached:", 11))) {
            idx = &buf[11];
            while (*idx == ' ') idx++;
            free = strtoull(idx, &hold, 10);
            rv -= free;
        } else {
            break;
        }
    } while (!feof(f));
    if (f != NULL) fclose(f);
    return rv;
}

/* Example of the first four lines of /proc/stat:

cpu  613674 711 269155 65079320 98418 1177 7073 0
cpu0 277159 473 131505 32522884 96873 1177 4698 0
cpu1 336514 237 137649 32556435 1545 0 2375 0
intr 3080936 252 242401 0 0 0 0 0 0 1 0 0 0 3 0 37 0 1 0 53 [... 200 more words]

*/

int readstat(uint64_t *d) {
    FILE *f;
    char buf[BUFSIZ];
    char *idx;
    int i, ncpu = 0;

    f = fopen("/proc/stat", "r");
    if (f == NULL) {
        perror("fopen");
        return -1;
    }

    /* The top CPU line is all we need, since we'll be doing our own math. */
    do {
        fgets(buf, BUFSIZ, f);
        buf[BUFSIZ - 1] = 0;
    } while (strncmp(buf, "cpu ", 4) && !feof(f));
    if (strncmp(buf, "cpu ", 4)) {
        fputs("Ran out of data in /proc/stat.\n", stderr);
        fclose(f);
        return -2;
    }

    strtok(buf, " \t");
    for (i = 0; i < S_TOT; i++) {
        idx = strtok(NULL, " \t");
        if (idx == NULL) {
            fputs("Malformed line in /proc/stat, data damaged.\n", stderr);
            fclose(f);
            return -3;
        }
        *d++ = strtoull(idx, NULL, 10);
    }

    do {
        fgets(buf, BUFSIZ, f);
        buf[BUFSIZ - 1] = 0;
        ncpu++;
    } while (!(strncmp(buf, "cpu", 3) || feof(f)));
    if (strncmp(buf, "cpu", 3)) {
        ncpu--;
    }
    fclose(f);
    return ncpu;
}
#endif

long tvdiff(struct timeval *start, struct timeval *end) {
    struct timeval tv;
    long rv;

    timersub(end, start, &tv);
    rv = tv.tv_sec * 1000000 + tv.tv_usec;
    return rv;
}

#ifdef SHOW_STATISTICS
void *watchstat(void *arg) {
    uint64_t orig[S_MAX], data[S_MAX];
#ifdef REPORT_MEM
    uint64_t mem;
#endif
    struct wire otw;
    int i;
    fd_set *f = (fd_set *)arg;
    struct timeval tv, tv2;
    long timediff;
    int ncpu;
    uint16_t h;

    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
    }
    ncpu = readstat(orig);
    if (ncpu < 1) {
        perror("readstat");
        return (void *)1;
    }
    orig[S_TOT] = 0;
    for (i = 0; i < S_TOT; i++) {
	if (i != S_IDLE) {
	    orig[S_TOT] += orig[i] / ncpu;
	}
    }
    for (;;) {
        if (gettimeofday(&tv2, NULL) < 0) {
            perror("gettimeofday");
        }
        timediff = 1000000 - tvdiff(&tv, &tv2);
        if (timediff > 0) {
            usleep(timediff);
        }
        if (gettimeofday(&tv, NULL) < 0) {
            perror("gettimeofday");
        }

        ncpu = readstat(data);
        if (ncpu < 1) {
            perror("readstat failed, using result anyway");
            ncpu = 1; /* Not a lot we can do here. */
        }
        data[S_TOT] = 0;
        for (i = 0; i < S_TOT; i++) {
            if (i != S_IDLE) {
                data[S_TOT] += data[i] / ncpu;
            }
        }

        /* printf("User\tNice\tSys\tIdle\tI/O\tIRQ\tSIRQ\n");
        printf("%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
            orig[S_USER], orig[S_NICE], orig[S_SYS], orig[S_IDLE],
            orig[S_IO], orig[S_IRQ], orig[S_SOFTIRQ]);
        printf("%llu\t%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
            data[S_USER], data[S_NICE], data[S_SYS], data[S_IDLE],
            data[S_IO], data[S_IRQ], data[S_SOFTIRQ]);
        for (i = 0; i < S_TOT; i++) {
            printf("%3.3f%%\t",
                100.0 * (data[i] - orig[i]) / (data[S_TOT] - orig[S_TOT]));
        }
        putchar('\n'); */

        otw.sec = htonl(tv.tv_sec);
        otw.usec = htonl(tv.tv_usec);
        otw.saddr = 1;
        otw.daddr = 1;
        otw.sport = htons(1);
        otw.dport = htons(1);
        otw.flowid = htonl(FLOWID_CPU);
        h = (uint16_t)((data[S_TOT] - orig[S_TOT]) & 0xFFFF);
        otw.len = htons(h);
        otw.proto = 61; /* 61 is any host-internal protocol */

        logpkt(&otw, NULL);
        pthread_mutex_lock(&biglock);
        for (i = 0; i < FD_SETSIZE; i++) if (FD_ISSET(i, f)) {
            if (send(i, &otw, sizeof (otw), MSG_NOSIGNAL) < 0) {
                perror("send error");
                FD_CLR(i, f);
                shutdown(i, SHUT_RDWR);
                close(i);
            }
        }
        pthread_mutex_unlock(&biglock);

#ifdef REPORT_MEM
        mem = readmem();
        otw.saddr = 2;
        otw.daddr = 2;
        otw.sport = htons(2);
        otw.dport = htons(2);
        otw.flowid = htonl(FLOWID_MEM);
        otw.len = htons((uint16_t)(mem >> 8));
        otw.proto = 61; /* 61 is any host-internal protocol */

        logpkt(&otw, NULL);
        pthread_mutex_lock(&biglock);
        for (i = 0; i < FD_SETSIZE; i++) if (FD_ISSET(i, f)) {
            if (send(i, &otw, sizeof (otw), MSG_NOSIGNAL) < 0) {
                perror("send error");
                FD_CLR(i, f);
                shutdown(i, SHUT_RDWR);
                close(i);
            }
        }
        pthread_mutex_unlock(&biglock);
#endif
        memcpy(orig, data, S_MAX * sizeof (uint64_t));
    }
    return (void *)2;
}
#endif

int main(int argc, char *argv[]) {
    pcap_t *pch;
    char errbuf[PCAP_ERRBUF_SIZE];
    const u_char *packet;
    struct pcap_pkthdr head;
    int i;
    int one = 1;
    struct linger l = { 0, 0 };
    int sock;
    struct sockaddr_in sin, lan;
    pthread_t ptaccess;
#ifdef SHOW_STATISTICS
    pthread_t ptcpu;
#endif
    bpf_u_int32 ip, mask;
    struct bpf_program filter;
    char filterstr[64];
    char listendev[IFNAMSIZ] = LISTENDEV;
#if 0
    char serverdev[IFNAMSIZ] = SERVERDEV;
#endif
    uint16_t port = PCAPPER_PORT;
    char mac[MACLEN];
    int mode = MODE_SRC;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) {
            if (++i < argc) {
                port = atoi(argv[i]);
                if (!port) {
                    fputs("Invalid port specified with -p.\n", stderr);
                    return 15;
                }
            } else {
                fputs("You must specify a port when using -p.\n", stderr);
                return 14;
            }
#if 0
        } else if (!strcmp(argv[i], "-s")) {
            if (++i < argc) {
                strncpy(serverdev, argv[i], IFNAMSIZ);
                serverdev[IFNAMSIZ - 1] = 0;
            } else {
                fputs("You must specify an interface name when using -s.\n",
                    stderr);
                return 9;
            }
#endif
        } else if (!strcmp(argv[i], "-f")) {
            if (++i < argc) {
                logfn = argv[i];
            } else {
                fputs("You must specify a logfile name when using -f.\n",
                    stderr);
                return 18;
            }
        } else if (!strcmp(argv[i], "-i")) {
            if (++i < argc) {
                strncpy(listendev, argv[i], IFNAMSIZ);
                listendev[IFNAMSIZ - 1] = 0;
            } else {
                fputs("You must specify an interface name when using -i.\n",
                    stderr);
                return 13;
            }
        } else if (!strcmp(argv[i], "-m")) {
            if (++i < argc) {
                if (!strcmp(argv[i], "src")) mode = MODE_SRC;
                else if (!strcmp(argv[i], "dst")) mode = MODE_DST;
                else if (!strcmp(argv[i], "spoof")) mode = MODE_SPOOF;
                else {
                    fprintf(stderr, "Invalid mode `%s' - valid modes are src, "
                        "dst, or spoof.\n", argv[i]);
                    return 19;
                }
            } else {
                fputs("You must specify a mode when using -m.\n", stderr);
                return 20;
            }
        } else {
            fprintf(stderr, "Usage: %s [-f logfile] [-p port] "
                "[-i interface] [-m <src | dst | spoof>]\n", argv[0]);
            return 12;
        }
    }

    if ((i = manhandle(mac, NULL, &lan.sin_addr, NULL, listendev, NULL))) {
        fprintf(stderr, "Could not get IP and MAC address for %s: %s\n",
            listendev, strerror(i));
        return 16;
    }

    if (mode == MODE_SRC) {
        sprintf(filterstr, SRCFILTER, mac);
    } else if (mode == MODE_DST) {
        sprintf(filterstr, DSTFILTER, mac);
    } else if (mode == MODE_SPOOF) {
        sprintf(filterstr, SPOFILTER,
            inet_ntoa((struct in_addr)lan.sin_addr), mac);
    } else {
        fprintf(stderr, "ERROR: Illegal mode %02X\n", mode);
        return 21;
    }

    if (pcap_lookupnet(listendev, &ip, &mask, errbuf) < 0) {
        printf("Couldn't get network or mask for %s: %s\n", listendev, errbuf);
        return 5;
    }

    pch = pcap_open_live(listendev, SNAP_LEN, PROMISC_MODE, TIMEOUT_MS, errbuf);
    if (pch == NULL) {
        printf("Couldn't open device %s: %s\n", listendev, errbuf);
        return 4;
    }

    if (pcap_compile(pch, &filter, filterstr, 1, mask) < 0) {
        printf("Could not compile filter `%s': %s\n", filterstr,
            pcap_geterr(pch));
        pcap_close(pch);
        return 6;
    }
    if (pcap_setfilter(pch, &filter) < 0) {
        printf("Could not apply filter: %s\n", pcap_geterr(pch));
        pcap_close(pch);
        return 7;
    }

#if 0
    if ((i = getip(&sin.sin_addr, serverdev, NULL))) {
        fprintf(stderr, "Could not get IP address for %s: %s\n",
            serverdev, strerror(i));
        pcap_close(pch);
        return 17;
    }
#endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        pcap_close(pch);
        return 1;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&l,
        sizeof (struct linger)) < 0) {
        perror("setsockopt linger");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&one,
        sizeof (int)) < 0) {
        perror("setsockopt keepalive");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
        sizeof (int)) < 0) {
        perror("setsockopt reuseaddr");
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&one,
        sizeof (int)) < 0) {
        perror("setsockopt reuseport");
    }
#endif
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&sin, sizeof (struct sockaddr_in)) < 0) {
        perror("bind");
        close(sock);
        pcap_close(pch);
        return 2;
    } else if (listen(sock, 5) < 0) {
        perror("listen");
        close(sock);
        pcap_close(pch);
        return 3;
    }

    if (pthread_create(&ptaccess, NULL, acceptor, (void *)(long)sock) < 0) {
        perror("pthread_create");
        close(sock);
        pcap_close(pch);
        return 8;
    } else {
        pthread_detach(ptaccess);
    }

#ifdef SHOW_STATISTICS
    if (pthread_create(&ptcpu, NULL, watchstat, (void *)&bigfs) < 0) {
        perror("pthread_create CPU");
        goto abend;
    } else {
        pthread_detach(ptcpu);
    }
#endif

    for (;;) {
        packet = pcap_next(pch, &head);
        if (packet != NULL) {
#if 0
            dumppkt(packet);
#endif
            blastpacket(&bigfs, packet, &head);
        }
    }
#ifdef SHOW_STATISTICS
    pthread_cancel(ptcpu);
abend:
#endif
    pthread_cancel(ptaccess);
    pcap_close(pch);
    for (i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, &bigfs)) {
            shutdown(i, SHUT_RDWR);
            close(i);
        }
    }
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return 0;
}
