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
#define MEMBUFSIZE 1000000

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
	int sum_len = 0;
	int read_len;
	while(sum_len < 8)
	{
		if ((read_len = read(sockfd, buf+sum_len, 8-sum_len)) == -1)
			perror("read");
		sum_len += read_len;
	}

	//check phase 1 response
	for (i = 0; i < 8; i++)
	{
		fprintf(stderr, "%02x ", buf[i]);
	}
	fprintf(stderr, "\n");

	//get protocol
	int protocol = buf[1];
	if (protocol != 1 && protocol != 2)
	{
		perror("protocol error");
		exit(1);
	}


	//write phase 2

	unsigned char *wbuf = malloc(MEMBUFSIZE);
	unsigned char ch;
	unsigned char* cp;

	if (protocol == 1)
	{	
		cp = wbuf;
		int read_len;
		int length = 0;
		int flag_esc = 0;
		int sent_len = 0;
		while (1)
		{
			if (flag_esc)
			{
				*cp = '\\';
				cp ++;
				length++;
				flag_esc = 0;
			}
			else
			{
				if ((read_len = read(STDIN_FILENO, &ch, 1)) == -1)
					perror("read");
				if (read_len == 0)
				{
					break;
				}
				*cp = ch;
				cp++;
				length++;
				if (ch == '\\')
				{
					flag_esc = 1;
				}
			}
			//if buffer is full, send and start to rewrite on buffer
			if (cp == wbuf + MEMBUFSIZE)
			{
				if (write(sockfd, wbuf, MEMBUFSIZE) == -1)
					perror("write");
				cp = wbuf;
				sent_len += MEMBUFSIZE;
				fprintf(stderr, "sent %d bytes of message\n", sent_len);
			}
		}
		//send remaining buffer content
		if (write(sockfd, wbuf, length % MEMBUFSIZE) == -1)
			perror("write");
		sent_len += (cp - wbuf);
		fprintf(stderr, "sent %d bytes of message\n", sent_len);
		
		//send terminator bytes
		wbuf[0] = '\\';
		wbuf[1] = '0';
		if (write(sockfd, wbuf, 2) == -1)
			perror("write");
		sent_len += 2;
		fprintf(stderr, "sent total %d bytes\n", sent_len);
	}

	if (protocol == 2)
	{
		//get message length of stdin
		int msg_len;
		msg_len = lseek(STDIN_FILENO, 0, SEEK_END);
		lseek(STDIN_FILENO, 0, SEEK_SET);
		fprintf(stderr, "msg_len = %d\n", msg_len);
	
		//send length field
		void *p;
		p = wbuf;
		*(int*)p = htonl(msg_len);
		if (write(sockfd, wbuf, 4) == -1)
				perror("write");
		
		//send string field
		int read_len;
		int toread_len;
		int sum_len = 0;
		while (sum_len < msg_len)
		{
			toread_len = (msg_len - sum_len) > MEMBUFSIZE ? MEMBUFSIZE : (msg_len - sum_len);
			if ((read_len = read(STDIN_FILENO, wbuf, toread_len)) == -1)
				perror("read");
			sum_len += read_len;
			if (write(sockfd, wbuf, read_len) == -1)
				perror("write");
		}
		fprintf(stderr, "length = %d, total sent = %d\n", msg_len, sum_len);
	}
	free(wbuf);


	//read phase 2

	unsigned char rbuf[BUFSIZE];

	if (protocol == 1)
	{
		cp = rbuf;
		int read_len = 0;
		int flag_esc = 0;
		int flag_terminate = 0;
		int sum_len = 0;
		while (!flag_terminate)
		{
			//read from socket to buffer
			if ((read_len = read(sockfd, &rbuf, BUFSIZE)) == -1)
				perror("read");
			sum_len += read_len;
			//write each chararacter from buffer to stdout
			for (cp = rbuf; cp < rbuf + read_len; cp++)
			{
				ch = *cp;
				if (flag_esc)
				{
					if (ch == '0')
					{
						flag_terminate = 1;
						break;
					}
					else if (ch == '\\')
					{
						write(STDOUT_FILENO, &ch, 1);
						flag_esc = 0;
						continue;
					}
		   		}
				else if (ch == '\\')
				{
					flag_esc = 1;
					continue;
				}
				else
				{
					write(STDOUT_FILENO, &ch, 1);
					flag_esc = 0;
				}
			}
		}
		fprintf(stderr, "received total %d bytes\n", sum_len);
	}

	else if (protocol == 2)
	{
		int read_len;
		
		//get length field
		int sum_len = 0;
		while(sum_len < 4)
		{
			if ((read_len = read(sockfd, rbuf+sum_len, 4-sum_len)) == -1)
				perror("read");
			sum_len += read_len;
		}

		int msg_len;
		void *vp;
		vp = rbuf;
		msg_len = ntohl(*(int*)vp);
		fprintf(stderr, "msg_len = %d\n", msg_len);
		
		//get string field
		int chunk_len;
		sum_len = 0;
		while(sum_len < msg_len)
		{
			chunk_len = (msg_len - sum_len) > BUFSIZE ? BUFSIZE : (msg_len - sum_len);
			if ((read_len = read(sockfd, rbuf, chunk_len)) == -1)
				perror("read");	
			sum_len += read_len;
			write(STDOUT_FILENO, rbuf, read_len);		
		}
		fprintf(stderr, "received %d out of %d bytes\n", sum_len, msg_len);
	
	}

	close(sockfd);
	return 0;
}


