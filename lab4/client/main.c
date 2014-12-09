// Lab4 Client
// Faraz Fallahi (faraz@siu.edu)
// with DNS & IPv6 support

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT   "3060"
#define SECRET "<cs306rembash>"

#define BUFFERSIZE 64 * 1024

void readline(int sockfd, char *buf)
{
    int pos = 0;
    char c;
    for(;;)
    {
        int n = recv(sockfd, &c, 1, 0);
        if(n < 1) break;
        buf[pos++] = c;
        if(c == '\n') break;
    }
    buf[pos] = '\0';
}

void sigint_handler(int signum)
{
   printf("\n\nCaught signal: %d\n\n", signum);
   exit(signum);
}

void print_addrinfo(struct addrinfo *input)
{
    int addr_i = 0;
    for(struct addrinfo *p = input; p != NULL; p = p->ai_next)
    {
        char *ipver;
        void *addr;

        // get the pointer to the address itself, different fields in IPv4 and IPv6
        if (p->ai_family == AF_INET)
        {
            ipver = "IPv4";
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else
        {
            ipver = "IPv6";
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr)); // convert the IP to a string
        printf("%2d. %s: %s\n", ++addr_i, ipver, ipstr);
    }
}

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        fprintf(stderr,"usage: %s hostname\n", argv[0]);
        return 1;
    }

    signal(SIGINT, sigint_handler);

    printf("Looking up addresses for %s ...\n", argv[1]);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *dnsres;
    int status = getaddrinfo(argv[1], PORT, &hints, &dnsres);
    if(status != 0)
    {
        fprintf(stderr, "dns lookup failed: %s\n", gai_strerror(status));
        return 2;
    }

    print_addrinfo(dnsres);

    printf("Connecting to %s ...\n", "the server");
    int sockfd = socket(dnsres->ai_family, dnsres->ai_socktype, dnsres->ai_protocol);

    if(connect(sockfd, dnsres->ai_addr, dnsres->ai_addrlen) != 0)
    {
        perror("connect");
        return 3;
    }
    printf("Connected.\n");

    freeaddrinfo(dnsres); // frees the memory that was dynamically allocated for the linked lists by getaddrinfo

    char buf[BUFFERSIZE + 1];
    int nbytes;

    readline(sockfd, buf);
    printf("Received: %s", buf);

    if(strcmp(buf, "<rembash>\n") != 0)
    {
        fprintf(stderr, "protocol failed\n");
        return 4;
    }

    send(sockfd, SECRET "\n", strlen(SECRET)+1, 0);

    readline(sockfd, buf);
    printf("Received: %s", buf);

    if(strcmp(buf, "<ok>\n") != 0)
    {
        fprintf(stderr, "shared key failed\n");
        return 5;
    }

    readline(sockfd, buf);
    printf("Received: %s", buf);

    if(strcmp(buf, "<ready>\n") != 0)
    {
        fprintf(stderr, "not ready!\n");
        return 6;
    }

    fd_set master, readfds;
    FD_ZERO(&master);
    FD_SET(STDIN_FILENO, &master);
    FD_SET(sockfd, &master);

    for(;;)
    {
        readfds = master;

        if(select(sockfd+1, &readfds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            return 7;
        }

        if(FD_ISSET(STDIN_FILENO, &readfds))
        {
            nbytes = read(STDIN_FILENO, buf, BUFFERSIZE);
            if(nbytes < 1) break;
            send(sockfd, buf, nbytes, 0);
        }

        if(FD_ISSET(sockfd, &readfds))
        {
            nbytes = recv(sockfd, buf, BUFFERSIZE, 0);
            if(nbytes < 1) break;
            write(STDOUT_FILENO, buf, nbytes);
        }
    }

    printf("\nBYE!\n");
    close(sockfd);
    return 0;
}
