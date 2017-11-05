#include <sys/socket.h>
#include <netinet/in.h>
#include<sys/types.h>
#include <unistd.h>
#include <errno.h>
#include<fcntl.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include<arpa/inet.h>
int main()
{
	int connfd;
	connfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(connfd == -1)
	{
		printf("socket creation failed");
		return 1;
	}
	/*
	struct timeval timeout;
	int len = sizeof(timeout);
	getsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, &len);
	printf("sec = %ld, usec = %ld\n", timeout.tv_sec, timeout.tv_usec);
	*/
	struct sockaddr_in addr;
	char sentence[1024];
	memset(&addr, 0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(6789);
	if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0)
       	{
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	if (connect(connfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
       	{
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	/*
	char *msg = "hello this is client 1";
	if(write(connfd, msg, strlen(msg)) < 0)
	{
		printf("error wirte\n");
		return 1;
	}
	struct timeval timeout;
	int len = sizeof(timeout);
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	if( setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, len) < 0)
	{
		printf("socket set failed\n");
		return 1;
	}
	*/
	char *greet = "hello";
	write(connfd, greet, strlen(greet));
	char response[1024];
	int n = read(connfd, response, 1024);
	if(n < 0)
	{
		printf("read failed");
	}
	for(int i = 0; i < n; ++i)
	{
		printf("%c", response[i]);
	}
	printf("\n");
	close(connfd);
	return 0;
}
