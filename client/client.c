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
#define BUFSIZE 1024

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
//now p is the addrinfo of the serverf
//inet_ntop, print the server name
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    fprintf(stderr, "connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    int i;
    void *vp;

    unsigned char op = 0;
    unsigned char proto = (unsigned char)(atoi(argv[6]) & 0xff);
    unsigned int trans_id = 0x00010000;
    unsigned short checksum;
    unsigned short opproto = (op << 8) + proto;
    unsigned int id1 = (trans_id >> 16) & 0xffff;
    unsigned int id2 = trans_id & 0xffff;
    unsigned int opprotoid1 = opproto + id1;
    if (opprotoid1 >> 16)
		opprotoid1 += 1;
    unsigned int sum = opprotoid1 + id2;
    if (sum >> 16)
		sum += 1;
    checksum = ~((unsigned short)sum);
    
    unsigned int firstline = htonl((opproto << 16) + checksum);
    unsigned int secondline = htonl(trans_id);
    unsigned int wbuf1[2];
    wbuf1[0] = firstline;
    wbuf1[1] = secondline;
    
    //check phase 1 request
    vp = wbuf1;
    for (i = 0; i < 8; i++)
    {
    	fprintf(stderr, "%02x ", *(unsigned char*)vp);
    	vp++;
    }
    fprintf(stderr, "\n");

	//write phase 1
    if (write(sockfd, wbuf1, 8) == -1)
		perror("write");
		
	//read phase 1
    if ((numbytes = read(sockfd, buf, 8)) == -1) {
        perror("read");
        exit(1);
    }
    
    //check phase 1 response
    for (i = 0; i < 8; i++)
    {
		fprintf(stderr, "%02x ", buf[i]);
    }
    fprintf(stderr, "\n");

    int protocol = buf[1];
    if (protocol != 1 && protocol != 2)
    {
    	perror("protocol error");
    	exit(1);
    }


		//write phase 2

	unsigned char wbuf[BUFSIZE];
	unsigned char ch;
	unsigned char* cp;

    if (protocol == 1)
    {	
		cp = wbuf;
		int length = 0;
    	int flag = 0;
    	int bytes = 0;
    	while (1)
		{
			if (flag)
			{
				*cp = '\\';
				cp ++;
				length++;
				flag = 0;
			}
			else
			{			
				int readchar;
				if ((readchar = read(STDIN_FILENO, &ch, 1)) == -1)
					perror("read");
				if (readchar == 0)
				{
					break;
				}
				*cp = ch;
				cp++;
				length++;
				if (ch == '\\')
				{
					flag = 1;
				}
			}
			if (cp == wbuf + BUFSIZE)
			{
				if (write(sockfd, wbuf, BUFSIZE) == -1)
					perror("write");
				cp = wbuf;
				bytes += BUFSIZE;
				fprintf(stderr, "sent total %d bytes\n", bytes);
			}
		}
		if (write(sockfd, wbuf, length % BUFSIZE) == -1)
			perror("write");
		bytes += (cp - wbuf);
		fprintf(stderr, "sent total %d bytes\n", bytes);
		
        wbuf[0] = '\\';
        wbuf[1] = '0';
		if (write(sockfd, wbuf, 2) == -1)
			perror("write");
		bytes += 2;
		fprintf(stderr, "sent total %d bytes\n", bytes);
    }
    if (protocol == 2)
    {
    	FILE *fp;
    	int length;
    	fp = fopen("temp", "w+");
    	while((length = fread(wbuf, 1, 1024, stdin)) > 0)
    	{
    		fwrite(wbuf, 1, length, fp);
    	}
    	
    	int filesize;
    	fseek(fp, 0, SEEK_END);
    	filesize = ftell(fp);
    	fseek(fp, 0, SEEK_SET);
    	
    	void *p;
    	p = wbuf;
    	*(int*)p = htonl(filesize);
    	if (write(sockfd, wbuf, 4) == -1)
    			perror("write");
    	int read_len;
    	while (read_len < filesize)
    	{
    		int read_add = (filesize - read_len) > BUFSIZE ? BUFSIZE : (filesize - read_len);
    		read_len += fread(wbuf, 1, read_add, fp);
    		if (write(sockfd, wbuf, read_add) == -1)
    			perror("write");
    	}
    	fclose(fp);    	
    }
    fprintf(stderr, "sent message\n");
    

	//read phase 2
    
    unsigned char rbuf[BUFSIZE];
    
    if (protocol == 1)
    {
		cp = rbuf;
		int readbytes = 0;
		int flag = 0;
		int terminate = 0;
		while (!terminate)
		{
			if ((readbytes = read(sockfd, &rbuf, BUFSIZE)) == -1)
			{
				perror("read");
			}
			if (readbytes == 0)
			{
				fprintf(stderr, "readbytes == 0\n");
				break;
			}
			for (cp = rbuf; cp < rbuf + BUFSIZE; cp++)
			{
				ch = *cp;
				if (flag)
				{
					if (ch == '0')
					{
						terminate = 1;
						break;
					}
					else if (ch == '\\')
					{
						write(STDOUT_FILENO, &ch, 1);
						flag = 0;
						continue;
					}
		   		}
				else if (ch == '\\')
				{
					flag = 1;
					continue;
				}
				else
				{
					write(STDOUT_FILENO, &ch, 1);
					flag = 0;
				}
			}
			if (terminate)
				break;
		}
	}
	
	else if (protocol == 2)
	{
		int recv_len = 0;
		while(recv_len < 4)
		{
			recv_len += read(sockfd, rbuf+recv_len, 4-recv_len);
		}
		int msglength;
    	void *vp;
    	vp = rbuf;
    	msglength = ntohl(*(int*)vp);
    	recv_len = 0;
    	while(recv_len < msglength)
    	{
    		int recv_add = (msglength - recv_len) > BUFSIZE ? BUFSIZE : (msglength - recv_len);
    		recv_len += read(sockfd, rbuf, recv_add);
    		write(STDOUT_FILENO, rbuf, recv_add);
    	}
    	
	}

    close(sockfd);
    return 0;
}


