/*
    mpg321 - a fully free clone of mpg123.
    Copyright (C) 2001 Joe Drew
    
    Network code based heavily upon:
    plaympeg - Sample MPEG player using the SMPEG library
    Copyright (C) 1999 Loki Entertainment Software
    
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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define _LARGEFILE_SOURCE 1

#include "mpg321.h"

#include <stdio.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <unistd.h>

#include <limits.h>

int icy_metaint = 0;

int is_address_multicast(unsigned long address)
{
    if ((address & 255) >= 224 && (address & 255) <= 239)
        return (1);
    return (0);
}

int tcp_open(char *address, int port)
{
    struct sockaddr_in stAddr;
    struct hostent *host;
    int sock;
    struct linger l;

    memset(&stAddr, 0, sizeof(stAddr));
    stAddr.sin_family = AF_INET;
    stAddr.sin_port = htons(port);

    if ((host = gethostbyname(address)) == NULL)
        return (0);

    stAddr.sin_addr = *((struct in_addr *)host->h_addr_list[0]);

    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return (0);

    l.l_onoff = 1;
    l.l_linger = 5;
    if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (char *)&l, sizeof(l)) < 0)
        return (0);

    if (connect(sock, (struct sockaddr *)&stAddr, sizeof(stAddr)) < 0)
        return (0);

    return (sock);
}

int udp_open(char *address, int port)
{
    int enable = 1L;
    struct sockaddr_in stAddr;
    struct sockaddr_in stLclAddr;
    struct ip_mreq stMreq;
    struct hostent *host;
    int sock;

    stAddr.sin_family = AF_INET;
    stAddr.sin_port = htons(port);

    if ((host = gethostbyname(address)) == NULL)
        return (0);

    stAddr.sin_addr = *((struct in_addr *)host->h_addr_list[0]);

    /* Create a UDP socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return (0);

    /* Allow multiple instance of the client to share the same address and port */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(unsigned long int)) < 0)
        return (0);

    /* If the address is multicast, register to the multicast group */
    if (is_address_multicast(stAddr.sin_addr.s_addr))
    {
        /* Bind the socket to port */
        stLclAddr.sin_family = AF_INET;
        stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        stLclAddr.sin_port = stAddr.sin_port;
        if (bind(sock, (struct sockaddr *)&stLclAddr, sizeof(stLclAddr)) < 0)
            return (0);

        /* Register to a multicast address */
        stMreq.imr_multiaddr.s_addr = stAddr.sin_addr.s_addr;
        stMreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&stMreq, sizeof(stMreq)) < 0)
            return (0);
    }
    else
    {
        /* Bind the socket to port */
        stLclAddr.sin_family = AF_INET;
        stLclAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        stLclAddr.sin_port = htons(0);
        if (bind(sock, (struct sockaddr *)&stLclAddr, sizeof(stLclAddr)) < 0)
            return (0);
    }

    return (sock);
}

int raw_open(char *arg)
{
    char *host;
    int port;
    int sock;

    /* Check for URL syntax */
    if (strncmp(arg, "raw://", strlen("raw://")))
        return (0);

    /* Parse URL */
    port = 0;
    host = arg + strlen("raw://");
    if (strchr(host, ':') != NULL)  /* port is specified */
    {
        port = atoi(strchr(host, ':') + 1);
        *strchr(host, ':') = 0;
    }

    /* Open a UDP socket */
    if (!(sock = udp_open(host, port)))
        perror("raw_open");

    return (sock);
}

/**
 * Read a http line header.
 * This function read character by character.
 * @param tcp_sock the socket use to read the stream
 * @param buf a buffer to receive the data
 * @param size size of the buffer
 * @return the size of the stream read or -1 if an error occured or 0 on EOF
 */
static int http_read_line(int tcp_sock, char *buf, int size)
{
    int offset = 0;
    int bytes_read;

    do
    {
        if ((bytes_read = read(tcp_sock, buf + offset, 1)) < 0)
            return -1;
        if (bytes_read == 0)
            return 0;
        if (buf[offset] != '\r')    /* Strip \r from answer */
            offset++;
    }
    while (offset < size - 1 && buf[offset - 1] != '\n');

    buf[offset] = 0;
    return offset;
}

int http_open(char *arg)
{
    char *host;
    int port;
    char *request;
    int tcp_sock;
    char http_request[PATH_MAX];
    char filename[PATH_MAX];
    char c;

    /* Check for URL syntax */
    if (strncmp(arg, "http://", strlen("http://")))
        return (0);

    /* Parse URL */
    port = 80;
    host = arg + strlen("http://");
    if (request = strchr(host, '/'))
    {
        *request++ = 0;
        snprintf(filename, sizeof(filename) - strlen(host) - 75, "/%s", request);
    } 
    else 
    {
        sprintf(filename, "/");
        request = filename;
    }

    if (strchr(host, ':') != NULL)  /* port is specified */
    {
        port = atoi(strchr(host, ':') + 1);
        *strchr(host, ':') = 0;
    }

    /* Open a TCP socket */
    if (!(tcp_sock = tcp_open(host, port)))
    {
        perror("http_open");
        return (0);
    }

    snprintf(filename, sizeof(filename) - strlen(host) - 75, "%s", request);

    /* Send HTTP GET request */
    /* Please don't use a Agent know by shoutcast (Lynx, Mozilla) seems to be reconized and print
     * a html page and not the stream */
    snprintf(http_request, sizeof(http_request),
    "GET /%s HTTP/1.0\r\n"
    "Pragma: no-cache\r\n"
    "Host: %s\r\n"
    "User-Agent: xmms/1.2.7\r\n" /* to make ShoutCast happy */
    "%s" /* to get metadata on ShoutCast stream */
    "Accept: */*\r\n"
    "\r\n", filename, host, (options.opt & MPG321_REMOTE_PLAY) ? "Icy-MetaData:1\r\n" : "");

    send(tcp_sock, http_request, strlen(http_request), 0);

    /* Parse server reply */
#if 0
    do
        read(tcp_sock, &c, sizeof(char));
    while (c != ' ');
    read(tcp_sock, http_request, 4 * sizeof(char));
    http_request[4] = 0;
    if (strcmp(http_request, "200 "))
    {
        fprintf(stderr, "http_open: ");
        do
        {
            read(tcp_sock, &c, sizeof(char));
            fprintf(stderr, "%c", c);
        }
        while (c != '\r');
        fprintf(stderr, "\n");
        return (0);
    }
#endif

    do
    {
        int len;

        len = http_read_line(tcp_sock, http_request, sizeof(http_request));

        if (len == -1)
        {
            fprintf(stderr, "http_open: %s\n", strerror(errno));
            return 0;
        }

        if (strncmp(http_request, "Location:", 9) == 0)
        {
            /* redirect */
            close(tcp_sock);
            
            http_request[strlen(http_request) - 1] = '\0';
            
            return http_open(&http_request[10]);
        }

        else if (strncmp(http_request, "ICY ", 4) == 0)
        {
            /* This is icecast streaming */
            if (strncmp(http_request + 4, "200 ", 4))
            {
                fprintf(stderr, "http_open: %s\n", http_request);
                return 0;
            }
        }
        else if ((options.opt & MPG321_REMOTE_PLAY) && (strncmp(http_request, "icy-metaint:", 12) == 0))
        {
            icy_metaint = atoi(http_request + 12);
            icy_buf_read = 0;
            icy_tag_crossed_boundary = 0;
        }
    }
    while (strcmp(http_request, "\n") != 0);

    return (tcp_sock);
}

int ftp_get_reply(int tcp_sock)
{
    int i;
    char c;
    char answer[1024];

    do
    {
        /* Read a line */
        for (i = 0, c = 0; i < 1024 && c != '\n'; i++)
        {
            read(tcp_sock, &c, sizeof(char));
            answer[i] = c;
        }
        answer[i] = 0;
        fprintf(stderr, "%s", answer + 4);
    }
    while (answer[3] == '-');

    answer[3] = 0;

    return (atoi(answer));
}

int ftp_open(char *arg)
{
    char *host;
    int port;
    char *dir;
    char *file;
    int tcp_sock;
    int data_sock;
    char ftp_request[PATH_MAX];
    struct sockaddr_in stLclAddr;
    socklen_t namelen;
    int i;

    /* Check for URL syntax */
    if (strncmp(arg, "ftp://", strlen("ftp://")))
        return (0);

    /* Parse URL */
    port = 21;
    host = arg + strlen("ftp://");
    if ((dir = strchr(host, '/')) == NULL)
        return (0);
    *dir++ = 0;
    if ((file = strrchr(dir, '/')) == NULL)
    {
        file = dir;
        dir = NULL;
    }
    else
        *file++ = 0;

    if (strchr(host, ':') != NULL)  /* port is specified */
    {
        port = atoi(strchr(host, ':') + 1);
        *strchr(host, ':') = 0;
    }

    /* Open a TCP socket */
    if (!(tcp_sock = tcp_open(host, port)))
    {
        perror("ftp_open");
        return (0);
    }

    /* Send FTP USER and PASS request */
    ftp_get_reply(tcp_sock);
    sprintf(ftp_request, "USER anonymous\r\n");
    send(tcp_sock, ftp_request, strlen(ftp_request), 0);
    if (ftp_get_reply(tcp_sock) != 331)
        return (0);
    sprintf(ftp_request, "PASS smpeguser@\r\n");
    send(tcp_sock, ftp_request, strlen(ftp_request), 0);
    if (ftp_get_reply(tcp_sock) != 230)
        return (0);
    sprintf(ftp_request, "TYPE I\r\n");
    send(tcp_sock, ftp_request, strlen(ftp_request), 0);
    if (ftp_get_reply(tcp_sock) != 200)
        return (0);
    if (dir != NULL)
    {
        snprintf(ftp_request, sizeof(ftp_request), "CWD %s\r\n", dir);
        send(tcp_sock, ftp_request, strlen(ftp_request), 0);
        if (ftp_get_reply(tcp_sock) != 250)
            return (0);
    }

    /* Get interface address */
    namelen = sizeof(stLclAddr);
    if (getsockname(tcp_sock, (struct sockaddr *)&stLclAddr, &namelen) < 0)
        return (0);

    /* Open data socket */
    if ((data_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
        return (0);

    stLclAddr.sin_family = AF_INET;

    /* Get the first free port */
    for (i = 0; i < 0xC000; i++)
    {
        stLclAddr.sin_port = htons(0x4000 + i);
        if (bind(data_sock, (struct sockaddr *)&stLclAddr, sizeof(stLclAddr)) >= 0)
            break;
    }
    port = 0x4000 + i;

    if (listen(data_sock, 1) < 0)
        return (0);

    i = ntohl(stLclAddr.sin_addr.s_addr);
    sprintf(ftp_request, "PORT %d,%d,%d,%d,%d,%d\r\n",
            (i >> 24) & 0xFF, (i >> 16) & 0xFF,
            (i >> 8) & 0xFF, i & 0xFF, (port >> 8) & 0xFF, port & 0xFF);
    send(tcp_sock, ftp_request, strlen(ftp_request), 0);
    if (ftp_get_reply(tcp_sock) != 200)
        return (0);

    snprintf(ftp_request, sizeof(ftp_request), "RETR %s\r\n", file);
    send(tcp_sock, ftp_request, strlen(ftp_request), 0);
    if (ftp_get_reply(tcp_sock) != 150)
        return (0);

    return (accept(data_sock, NULL, NULL));
}
