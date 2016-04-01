/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

//#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// ./client -h 000.000.000.000 -p 1234 -m 1
// host, port, protocol number
int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if (argc != 7 || strcmp(argv[1], "-h") != 0 || strcmp(argv[3], "-p") != 0 || strcmp(argv[5], "-m") != 0)
    {
	fprintf(stderr,"usage: client -h hostip -p port -m protocol\n");
        exit(1);
    }


//initialize hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

//get addrinfo of target address and port and hint info and put that into servinfo pointer, which is a linked list.
    if ((rv = getaddrinfo(argv[2], argv[4], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
//socket()
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
//connect()
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
//now p is the addrinfo of the server
//inet_ntop, print the server name
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    unsigned char op = 0;
    unsigned char proto = (unsigned char)(atoi(argv[6]) & 0xff);
    unsigned int trans_id = 0xffffffff;
    unsigned short checksum;
    unsigned int opproto = (op << 8) + proto;
    printf("op and proto = %hu\n", (unsigned short)opproto);
    unsigned int id1 = (trans_id >> 16) & 0xffff;
    printf("transid1 = %hu\n", (unsigned short)id1);
    unsigned int id2 = trans_id & 0xffff;
    printf("transid2 = %hu\n", (unsigned short)id2);
    unsigned int opprotoid1 = opproto + id1;
    if (opprotoid1 >> 16)
	opprotoid1 += 1;
    unsigned int sum = opprotoid1 + id2;
    if (sum >> 16)
	sum += 1;
    checksum = ~((unsigned short)sum);
    printf("checksum = %hu\n", checksum);
    //printf("sum+checksum = %hu\n", sum + checksum);
    
    int32_t firstline = htonl((opproto << 16) + checksum);
    int32_t secondline = htonl(trans_id);
    int buffer[8];
    buffer[0] = firstline;
    buffer[4] = secondline;
    
    if (send(sockfd, buffer, 8, 0) == -1)
	perror("send");

    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);
    int i;
    for (i = 0; i < 8; i++)
    {
	printf("%hu\n", ntohs((unsigned short)buf[i]) >> 8);
    }
    close(sockfd);

    return 0;
}

