#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BACKLOG 512
#define MAX_EVENTS 128
#define MAX_MESSAGE_LEN 2048


static void error(char* msg)
{
	perror(msg);
	printf("erreur...\n");
	exit(1);
}


static int create_http_response(char *buf, int len)
{
    memset(buf, 0, len);
    strcat(buf, "HTTP/1.1 200 OK\n");
    strcat(buf, "Content-type: text/html\n");
    strcat(buf, "Content-Length: 11\n");
    strcat(buf, "\n");
    strcat(buf, "Hello World");

    return strlen(buf);
}

static void non_block_accept_ctl(int sockfd, struct epoll_event *ev, int epollfd)
{
    int connfd;
 	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	connfd = accept4(sockfd, (struct sockaddr *)&client_addr, &client_len, SOCK_NONBLOCK);
    if (connfd == -1) {
        error("Error accepting new connection..\n");
    }

    ev->events = EPOLLIN | EPOLLET;
    ev->data.fd = connfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, ev) == -1)
    {
        error("Error adding new event to epoll..\n");
    }
}


static void echo_msg(int newfd, int epollfd)
{
	char buffer[MAX_MESSAGE_LEN];
    int bytes_rcv, resp_len;

	memset(buffer, 0, sizeof(buffer));
    bytes_rcv = recv(newfd, buffer, MAX_MESSAGE_LEN, 0);

    if (bytes_rcv <= 0) {
        epoll_ctl(epollfd, EPOLL_CTL_DEL, newfd, NULL);
        shutdown(newfd, SHUT_RDWR);
    } else {
        //printf("buf: %s\n", buffer);
        resp_len = create_http_response(buffer, sizeof(buffer));
        send(newfd, buffer, resp_len, 0);
    }
}


static int tcp_socket_create_bind(char **argv)
{
    int sockfd;
    struct sockaddr_in server_addr;
    unsigned int myport;

	if (argv[1])
		myport = atoi(argv[1]);
	else
		myport = 7838;

    printf("begin bind!\n");
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	} else {
		printf("socket created\n");
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(myport);
	if (argv[2])
		server_addr.sin_addr.s_addr = inet_addr(argv[2]);
	else
		server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))
		== -1) {
		perror("bind");		
		exit(1);
	} else
		printf("binded\n");

	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	} else
		printf("begin listen\n");
    
    return sockfd;
}


static int epoll_create_ctl(int sockfd, struct epoll_event *ev)
{
	int epollfd = epoll_create(MAX_EVENTS);
	if (epollfd < 0) {
		error("Error creating epoll..\n");
	}
	ev->events = EPOLLIN;
	ev->data.fd = sockfd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, ev) == -1) {
		error("Error adding new listeding socket to epoll..\n");
	}

    return epollfd;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Please give a port number and ip addr:    \
        ./epoll_echo_server [port] [ip addr]\n");
        exit(0);
    } 
    
	int sockfd, new_events, epollfd;
 	struct epoll_event ev, events[MAX_EVENTS];
	// some variables we need

    sockfd = tcp_socket_create_bind(argv);

	printf("epoll echo server listening for connections on port: %d\n", htons(atoi(argv[1])));


    epollfd = epoll_create_ctl(sockfd, &ev);

	while(1) {
		new_events = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		
		if (new_events == -1) {
			error("Error in epoll_wait..\n");
		}

		for (int i = 0; i < new_events; ++i) {
			if (events[i].data.fd == sockfd) {
                non_block_accept_ctl(sockfd, &ev, epollfd);
			}
			else {
                echo_msg(events[i].data.fd, epollfd);
			}
		}
	}

    return 0;
}
