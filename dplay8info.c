/*
    Copyright 2004,2005,2006 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl.txt
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "show_dump.h"

#ifdef WIN32
    #include <winsock.h>
    #include "winerr.h"

    #define close   closesocket
    #define sleep   Sleep
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <arpa/inet.h>
    #include <netdb.h>
#endif



#define VER     "0.1"
#define BUFFSZ  2048
#define PORT    6073
#define TIMEOUT 5
#define INFO    "\x00\x02"              /* info??? */ \
                "\xff\xff"              /* ID tracker */ \
                "\x02"                  /* 01 = need GUID */
                                        /* 02 = no GUID = cool */



void show_unicode(u_char *ptr, u_int size);
void timeout(int sock);
u_int resolv(char *host);
void std_err(void);



struct myguid {
    u_int  g1;
    u_short g2;
    u_short g3;
    u_char  g4;
    u_char  g5;
    u_char  g6;
    u_char  g7;
    u_char  g8;
    u_char  g9;
    u_char  g10;
    u_char  g11;
};

struct dplay8_struct {
    u_int  pcklen;
    u_int  reservedlen;
    u_int  flags;
    u_int  session;
    u_int  max_players;
    u_int  players;
    u_int  fulldatalen;
    u_int  desclen;
    u_int  dunno1;
    u_int  dunno2;
    u_int  dunno3;
    u_int  dunno4;
    u_int  datalen;
    u_int  appreservedlen;
    struct  myguid  instance_guid;
    struct  myguid  application_guid;
};



int main(int argc, char *argv[]) {
    int         sd,
                len,
                i,
                psz;
    u_short     port = PORT;
    u_char      *buff,
                hexdump = 0,
                verbose = 0;
    struct  sockaddr_in     peer;
    struct  dplay8_struct   *dplay8;

#ifdef WIN32
    WSADATA    wsadata;
    WSAStartup(MAKEWORD(1,0), &wsadata);
#endif


    setbuf(stdout, NULL);

    fputs("\n"
        "DirectPlay 8 Info "VER"\n"
        "by Luigi Auriemma\n"
        "e-mail: aluigi@autistici.org\n"
        "web:    aluigi.org\n"
        "\n", stdout);

    if(argc < 2) {
        printf("\n"
            "Usage: %s [options] <server>\n"
            "\n"
            "Options:\n"
            "-x        enable hexdump, show the entire packet sent by the server\n"
            "-v        verbose mode, shows all the values in the header (useless)\n"
            "-p PORT   change the destination port (default %d)\n"
            "\n", argv[0], PORT);
        exit(1);
    }

    argc--;
    for(i = 1; i < argc; i++) {
        switch(argv[i][1]) {
            case 'x': hexdump = 1; break;
            case 'v': verbose = 1; break;
            case 'p': port = atoi(argv[++i]); break;
            default: {
                printf("\nError: wrong command-line argument (%s)\n", argv[i]);
                exit(1);
                } break;
        }
    }

    peer.sin_addr.s_addr = resolv(argv[argc]);
    peer.sin_port        = htons(port);
    peer.sin_family      = AF_INET;
    psz                  = sizeof(peer);

    printf("\nContacting %s:%hu\n",
        inet_ntoa(peer.sin_addr), port);

    sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sd < 0) std_err();

    if(sendto(sd, INFO, sizeof(INFO) - 1, 0, (struct sockaddr *)&peer, psz)
      < 0) std_err();

    timeout(sd);

    buff = malloc(BUFFSZ);
    if(!buff) std_err();
    len = recvfrom(sd, buff, BUFFSZ, 0, (struct sockaddr *)&peer, &psz);
    if(len < 0) std_err();
    close(sd);

    if(len < (sizeof(struct dplay8_struct) + 4)) {
        printf("\nError: the packet received seems invalid\n");
        exit(1);
    }

    dplay8 = (struct dplay8_struct *)(buff + 4);

    if(dplay8->session) {
        fputs("\nSession options:     ", stdout);
        if(dplay8->session & 1) fputs("client-server ", stdout);
        if(dplay8->session & 4) fputs("migrate_host ", stdout);
        if(dplay8->session & 64) fputs("no_DPN_server ", stdout);
        if(dplay8->session & 128) fputs("password ", stdout);
    }

    printf("\n"
        "Max players          %u\n"
        "Current players      %u\n",
        dplay8->max_players,
        dplay8->players);

    if(verbose) {
        printf("\n"
            "Lengths:\n"
            " Reserved data       %u\n"
            " Full data           %u\n"
            " Description         %u\n"
            " Data                %u\n"
            " Application data    %u\n"
            "\n"
            "Unknown values:\n"
            " pcklen?             %u\n"
            " flags?              %u\n"
            " ???                 %u\n"
            " ???                 %u\n"
            " ???                 %u\n"
            " ???                 %u\n",
            dplay8->reservedlen,
            dplay8->fulldatalen,
            dplay8->desclen,
            dplay8->datalen,
            dplay8->appreservedlen,
            dplay8->pcklen, /* unknowns*/
            dplay8->flags,
            dplay8->dunno1,
            dplay8->dunno2,
            dplay8->dunno3,
            dplay8->dunno4);
    }

    printf("\n"
        "Instance GUID        %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        dplay8->instance_guid.g1,
        dplay8->instance_guid.g2,
        dplay8->instance_guid.g3,
        dplay8->instance_guid.g4,
        dplay8->instance_guid.g5,
        dplay8->instance_guid.g6,
        dplay8->instance_guid.g7,
        dplay8->instance_guid.g8,
        dplay8->instance_guid.g9,
        dplay8->instance_guid.g10,
        dplay8->instance_guid.g11);

    printf("\n"
        "Application GUID     %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        dplay8->application_guid.g1,
        dplay8->application_guid.g2,
        dplay8->application_guid.g3,
        dplay8->application_guid.g4,
        dplay8->application_guid.g5,
        dplay8->application_guid.g6,
        dplay8->application_guid.g7,
        dplay8->application_guid.g8,
        dplay8->application_guid.g9,
        dplay8->application_guid.g10,
        dplay8->application_guid.g11);


        // simple security checks
    if(
      (dplay8->datalen > len)        ||
      (dplay8->appreservedlen > len) ||
      ((dplay8->fulldatalen + dplay8->desclen + dplay8->reservedlen + 4) > (u_int)len)) {
        fputs("\nError: the packet seems damaged or hacked\n", stdout);
        exit(1);
    }

    fputs("\nGame description/Session name:\n\n  ", stdout);
    show_unicode(
        buff + 4 + dplay8->fulldatalen,
        len - dplay8->fulldatalen);

    if(dplay8->appreservedlen) {
        printf("\nHexdump of the APPLICATION RESERVED data (%u) sent by the server:\n\n",
            dplay8->appreservedlen);
        show_dump(
            buff + 4 + dplay8->datalen,
            dplay8->appreservedlen,
            stdout);
    }

    if(dplay8->reservedlen) {
        printf("\nHexdump of the RESERVED data (%u) sent by the server:\n\n",
            dplay8->reservedlen);
        show_dump(
            buff + 4 + dplay8->fulldatalen + dplay8->desclen,
            dplay8->reservedlen,
            stdout);
    }

    if(hexdump) {
        printf("\nHexdump of the complete packet (%d):\n\n", len);
        show_dump(buff, len, stdout);
    }

    fputc('\n', stdout);
    return(0);
}



void show_unicode(u_char *ptr, u_int size) {
    u_char  *limit = ptr + size;

    while(*ptr) {
        fputc(*ptr++, stdout);
        ptr++;
        if(ptr >= limit) break;
    }
    fputc('\n', stdout);
}



void timeout(int sock) {
    struct  timeval tout;
    fd_set  fd_read;
    int     err;

    tout.tv_sec = TIMEOUT;
    tout.tv_usec = 0;
    FD_ZERO(&fd_read);
    FD_SET(sock, &fd_read);
    err = select(sock + 1, &fd_read, NULL, NULL, &tout);
    if(err < 0) std_err();
    if(!err) {
        fputs("\nError: Socket timeout, no answers received\n", stdout);
        exit(1);
    }
}



u_int resolv(char *host) {
    struct hostent *hp;
    u_int host_ip;

    host_ip = inet_addr(host);
    if(host_ip == INADDR_NONE) {
        hp = gethostbyname(host);
        if(!hp) {
            printf("\nError: Unable to resolv hostname (%s)\n", host);
            exit(1);
        } else host_ip = *(u_int *)hp->h_addr;
    }
    return(host_ip);
}



#ifndef WIN32
    void std_err(void) {
        perror("\nError");
        exit(1);
    }
#endif



