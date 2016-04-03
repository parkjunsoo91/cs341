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

#define MAXDATASIZE 100 // max number of bytes we can get at once 

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
    int sockfd, numbytes;  
    unsigned char buf[MAXDATASIZE];
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

//get addrinfo of target address and port and hint info and put that into servinfo pointer, which is a linked list of addrinfo
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
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    int i;

    unsigned char op = 0;
    unsigned char proto = (unsigned char)(atoi(argv[6]) & 0xff);
    unsigned int trans_id = 0x00010000;
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
    
    int32_t firstline = htonl((opproto << 16) + checksum);
    int32_t secondline = htonl(trans_id);
    int buffer[2];
    buffer[0] = firstline;
    buffer[1] = secondline;

    if (write(sockfd, buffer, 8) == -1)
	perror("send");

    if ((numbytes = read(sockfd, buf, MAXDATASIZE-1)) == -1) {
        perror("recv");
        exit(1);
    }

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);
    for (i = 0; i < 8; i++)
    {
		printf("%02X,", buf[i]);
    }
    printf("\n");

    int protocol = buf[1];
    printf("protocol = %d\n", protocol);
    if (protocol != 1 && protocol != 2)
    {
    	perror("protocol error");
    }

	unsigned char wbuf[100] = "";
	unsigned char ch;
	unsigned char* cp;
	int length = 0;

    if (protocol == 1)
    {
    	cp = wbuf;
    	while (read(STDIN_FILENO, &ch, 1) > 0)
		{
			*cp = ch;
			cp++;
			length++;
		}
        wbuf[length] = '\\';
        wbuf[length + 1] = '0';
        
        printf("bytes sent, length = %d :\n", length + 2);
        for (i = 0; i < length + 2; i++)
		{
			printf("%02x ", wbuf[i]);
		}	
		printf("\n");
    }
    if (protocol == 2)
    {
    	cp = wbuf + 4;
    	while (read(STDIN_FILENO, &ch, 1) > 0)
		{
			*cp = ch;
			cp++;
			length++;
		}
    	void *p;
    	p = wbuf;
    	*(int*)p = htonl(length);

        printf("bytes sent, length = %d :\n", length + 4);
        for (i = 0; i < length + 4; i++)
		{
			printf("%02x ", wbuf[i]);
		}	
		printf("\n");
    }
    


//write string
    if (write(sockfd, wbuf, 100) == -1)
		perror("write");

//read response
    int readbytes;
    unsigned char rbuf[100];
    if ((readbytes = read(sockfd, rbuf, 100)) == -1)
        perror("read");
    
    printf("bytes received, length = %d: \n", readbytes);
	for (i = 0; i < readbytes; i++)
    {
        printf("%02x ", rbuf[i]);
    }
    printf("\n");
    
    
    
    if (protocol == 1)
    {
        printf("result string, length = %d: \n", readbytes - 2);
        for (i = 0; i<readbytes-2; i++)
    	{
    		printf("%02X ", rbuf[i]);
    	}
    	printf("\n");
    }
    else if (protocol == 2)
    {
    	int msglength;
    	void *vp;
    	vp = rbuf;
    	msglength = ntohl(*(int*)vp);
    	printf("msglength = %d\n", msglength);
    	printf("result string, length = %d: \n", msglength);
    	for (i = 4; i<msglength + 4; i++)
    	{
    		printf("%02x ", rbuf[i]);
    	}
    	printf("\n");
    }
    printf("\n");

    close(sockfd);
    return 0;
}


