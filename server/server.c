/*
** server.c -- a stream socket server demo
*/

#include <stdio.h>
#include <stdlib.h>
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

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10     // how many pending connections queue will hold

#define MAXDATASIZE 100 // max number of bytes we can get at once 

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char *port = PORT;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if (argc != 1 && (argc != 3 || strcmp(argv[1], "-p") != 0))
    {
        fprintf(stderr,"usage: server (-p port)\n");
        exit(1);
    }

    if(argc == 3) port = argv[2];

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
//socket()
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
//bind()
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }
//listen()
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    unsigned char op = 1;
    unsigned char proto;
    unsigned int trans_id;
    unsigned short checksum;
    unsigned int opproto;
    //printf("op and proto = %hu\n", (unsigned short)opproto);
    unsigned int id1;
    //printf("transid1 = %hu\n", (unsigned short)id1);
    unsigned int id2;
    //printf("transid2 = %hu\n", (unsigned short)id2);
    unsigned int opprotoid1;
    if (opprotoid1 >> 16) opprotoid1 += 1;
    unsigned int sum;
    unsigned char buf[MAXDATASIZE];
    int buffer[2];
    int i;

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        /*
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "Hello, world!", 13, 0) == -1)
                perror("send");
            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
        */
        else 
        {
            inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);
            printf("server: got connection from %s\n", s);
            //if (!fork()) { // this is the child process
                while(1)
                {
                    if(read(new_fd, buf, MAXDATASIZE-1) != -1)
                    {
                        for (i = 0; i < 8; i++)
                        {
                            printf("%hu,", *(buf+i));
                        }
                        //TODO: check invalid datas
                        //      (ex. op != 0, proto != 0 | 1 | 2, checksum failed)
                        printf("\n");
                        proto = (unsigned char)(*(buf+1));
                        printf("proto = %d\n", proto);
                        checksum = ntohs(*(unsigned short *)(buf+2));
                        printf("checksum = %d\n",checksum);
                        trans_id = ntohl(*(unsigned int *)(buf+4));
                        if(ntohs(*(unsigned short *)(buf))+
                            ntohs(*(unsigned short *)(buf+2))+
                            ntohs(*(unsigned short *)(buf+4))+
                            ntohs(*(unsigned short *)(buf+6)) == (unsigned short)-1) printf("checksum passed\n");
                        else printf("checksum failed\n");
                        printf("trans_id = %d\n", trans_id);
                        if(proto == 0) proto = 1; //It is for tests, change it randomly later.
                        opproto = (op << 8) + proto;
                        id1 = (trans_id >> 16) & 0xffff;
                        id2 = trans_id & 0xffff;
                        opprotoid1 = opproto + id1;
                        if (opprotoid1 >> 16) opprotoid1 += 1;
                        sum = opprotoid1 + id2;
                        if (sum >> 16) sum += 1;
                        checksum = ~((unsigned short)sum);
                        buffer[0] = htonl((opproto << 16) + checksum);
                        buffer[1] = htonl(trans_id);
                        if (write(new_fd, buffer, 8) == -1) perror("send");
                        break;
                    }
                }
                printf("pass point1\n");
                if(proto == 1)
                {
                    printf("pass point2\n");
                    // char prev;
                    // int init = 1;
                    int init = 1;
                    char buf2[MAXDATASIZE];
                    char prev;
                    while(1)
                    {
                        if(read(new_fd, buf, 1) != -1)
                        {
                            printf("%c\n", buf[0]);
                            if(init == 1)
                            {
                                printf("init\n");
                                prev = buf[0];
                                if(write(new_fd, buf, 1) == -1) perror("send");
                                init = 0;
                            }
                            else
                            {
                                if(buf[0]=='\\')
                                {
                                    printf("1\n");
                                    read(new_fd, buf+1, 1);
                                    if(buf[1] == '\\')
                                    {
                                        if(prev!='\\')
                                        {
                                            prev = '\\';
                                            if(write(new_fd, buf, 2) == -1) perror("send");
                                        }
                                    }
                                    else if(buf[1]=='0')
                                    {
                                        printf("terminate\n");
                                        if(write(new_fd, buf, 2) == -1) perror("send");
                                        break;
                                    }
                                    else
                                    {
                                        break;

                                    }
                                }
                                else
                                {
                                    printf("2\n");
                                    if(buf[0] != prev)
                                    {
                                        printf("2-not remove\n");
                                        if(write(new_fd, buf, 1) == -1) perror("send");
                                        prev = buf[0];
                                    }
                                }
                            }
                        }
                    }
                }
            //}
            close(new_fd);
        }
    }

    return 0;
}


