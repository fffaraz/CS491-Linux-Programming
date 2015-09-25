// Lab4 Server
// Faraz Fallahi (faraz@siu.edu)
// with IPv6 support

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define PORT   "5910"
#define SECRET "<cs591secret>"
#define BUFFERSIZE 64 * 1024
#define MAXEVENTS  64

struct EventData
{
    int fd;
    int idx;
};

union EventUnion
{
    EventData d;
    __uint64_t u64;
}

struct Client
{
    _Bool isvalid = 0;
    int state = 0;
    int socket = 0;
    int pty = 0;
    pid_t pid = 0;
    unsigned long bytes_recv = 0;
    unsigned long bytes_sent = 0;
};

#define MAXCLIENTS 100000
struct Client clients[MAXCLIENTS];

int new_client()
{
    static int idx = 0;
    while(clients[idx].isvalid) idx = (idx+1) % MAXCLIENTS;
    return idx;
}

void *get_in_addr(struct sockaddr *sa) // get sockaddr, IPv4 or IPv6:
{
    if(sa->sa_family == AF_INET) return &(((struct sockaddr_in*) sa)->sin_addr);
    else return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int setnonblocking(int sfd)
{
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) { perror("fcntl"); return -1; }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) { perror("fcntl"); return -1; }

    return 0;
}

int main(void)
{
    signal(SIGCHLD, SIG_IGN);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP. "| AI_ADDRCONFIG"
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_family = AF_INET6; // IPv4 addresses will be like ::ffff:127.0.0.1

    struct addrinfo *servinfo;
    getaddrinfo(NULL, PORT, &hints, &servinfo);

#if DEBUG
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next)
    {
        char ipstr[INET6_ADDRSTRLEN];
        inet_ntop(p->ai_family, get_in_addr(p->ai_addr), ipstr, sizeof(ipstr)); // convert the IP to a string
        printf(" %s\n", ipstr);
    }
#endif

    struct addrinfo *servinfo2 = servinfo; //servinfo->ai_next;
    char ipstr[INET6_ADDRSTRLEN];
    inet_ntop(servinfo2->ai_family, get_in_addr(servinfo2->ai_addr), ipstr, sizeof(ipstr));
    printf("Waiting for connections on [%s]:%s\n", ipstr, PORT);

    int sockfd = socket(servinfo2->ai_family, servinfo2->ai_socktype, servinfo2->ai_protocol);

#if 1
    int yes_1 = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes_1, sizeof(yes_1));
#endif

    bind(sockfd, servinfo2->ai_addr, servinfo2->ai_addrlen);
    freeaddrinfo(servinfo); // all done with this structure
    setnonblocking(sockfd);
    listen(sockfd, 10);

    int efd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = sockfd;
    epoll_ctl(efd, EPOLL_CTL_ADD, sockfd, &event);

    struct epoll_event events[MAXEVENTS];

    for(;;)
    {
        int nfd = epoll_wait(efd, events, MAXEVENTS, -1);
        for(int n = 0; n < nfd; ++n)
        {
            if(events[n].data.fd == sockfd) // listener
            {
                int idx = new_client();
                struct sockaddr_storage their_addr; // connector's address information
                socklen_t addr_size = sizeof(their_addr);
                clients[idx].socket = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
                setnonblocking(clients[idx].socket);

                char ipstr[INET6_ADDRSTRLEN];
                inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), ipstr, sizeof(ipstr));
                printf("Got a connection from %s [%d]\n", ipstr, clients[idx].socket);

                const char hello_msg[] = "<rembash2>\n";
                send(clients[idx].socket, hello_msg, sizeof(hello_msg), 0);

                EventData ed;
                ed.fd = clients[idx].socket;
                ed.idx = idx;
                EventUnion eu;
                eu.d = ed;
                event.events = EPOLLIN | EPOLLET;
                event.data.u64 = eu.u64;
                epoll_ctl(efd, EPOLL_CTL_ADD, ed.fd, &event);
            }
            else // client socket or pty
            {
                char buf[BUFFERSIZE];
                EventUnion eu;
                eu.u64 = events[n].data.u64;

                if(clients[eu.d.idx].state == 0)
                {
                    // assert(eu.d.fd == clients[eu.d.idx].socket)
                    int nbytes = recv(eu.d.fd, buff, 255, 0); // it's not 100% guaranteed to work! must use readline.
                    buff[nbytes - 1] = '\0';
                    printf("Received %s from [%d]\n", buf, eu.d.fd);

                    if(strcmp(buff, SECRET) != 0)
                    {
                        printf("Shared key check failed for [%d]\n", eu.d.idx);
                        //return; FIXME TODO
                    }

                    const char ok_msg[] = "<ok>\n";
                    send(clients[eu.d.idx].socket, ok_msg, sizeof(ok_msg), 0);
                    clients[eu.d.idx].state = 1;

                    clients[eu.d.idx].pid = forkpty(&clients[eu.d.idx].pty, NULL, NULL, NULL);

                    if(clients[eu.d.idx].pid == 0) // child
                    {
                        close(efd); // child doesn't need epoll
                        close(sockfd); // child doesn't need the listener
                        setsid();
                        execl("/bin/bash", "bash", NULL);
                        _exit(0);
                        return 0;
                    }
                    else
                    {
                        EventData ed;
                        ed.fd = clients[eu.d.idx].pty;
                        ed.idx = eu.d.idx;
                        EventUnion eu;
                        eu.d = ed;
                        event.events = EPOLLIN | EPOLLET;
                        event.data.u64 = eu.u64;
                        epoll_ctl(efd, EPOLL_CTL_ADD, ed.fd, &event);
                        const char ready_msg[] = "<ready>\n";
                        send(clients[eu.d.idx].socket, ready_msg, sizeof(ready_msg), 0);
                    }
                } // if(client->state == 0)
                else // if(client->state == 1)
                {
                    int nbytes = read(eu.d.fd, buf, BUFFERSIZE);
                    //if(nbytes < 1) break; TODO
                    if(eu.d.fd == clients[eu.d.idx].socket) write(clients[eu.d.idx].pty,    buf, nbytes);
                    if(eu.d.fd == clients[eu.d.idx].pty)    write(clients[eu.d.idx].socket, buf, nbytes);
                }
            } // if(events[n].data.fd == sockfd)
        } // for(int n = 0; n < nfd; ++n)
    } // for(;;)

    return 0;
}
