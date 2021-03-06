#include	"unp.h"
#include	"mash.h"
#include	<stdlib.h>
#include	<stdbool.h>
#include	<sys/epoll.h>

int epollfd = 0;
MASHDATA *coredata = NULL;
int message_seq = 0;

int main(int argc, char **argv)
{
	int i = 0;
	int sockfd = 0;
	int connfd = 0;
	int nread = 0;
	int number = 0;
	int listenfd = 0;
	int write_ret = 0;
	socklen_t addrlen;

	struct epoll_event events[MAX_EVENT_NUMBER];

	coredata = malloc(MAX_CLIENT_NUM * sizeof(MASHDATA));
	memset(coredata, '\0', MAX_CLIENT_NUM * sizeof(MASHDATA));
	listenfd = Tcp_listen(NULL, "19293", &addrlen);
	epollfd = Epoll_create( 5 );
	addevent(epollfd, listenfd, false);
	make_threadpool(10);
	log_serv("server started\n");
	/* loop process io events. */
	for ( ; ; ){
		if( (number = Epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 )) <= -1 )
			continue;
		for ( i = 0; i < number; i++ ){
			sockfd = events[i].data.fd;
			if( sockfd == listenfd ){
				/* If it is a connect event */
				struct sockaddr_in client_address;
				socklen_t client_addrlength = sizeof( client_address );
				connfd = Accept( listenfd, ( struct sockaddr* )&client_address, &client_addrlength );

				/* If Connect success initial the mash data struct */
				if( mash_init(coredata + connfd, connfd, client_address) > 0 )
					addevent(epollfd, connfd, false);
				continue;
			}
			if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) ){
				mash_close(coredata + sockfd);
				continue;
			}
			if( events[i].events & EPOLLIN ){
				threadpool_append(coredata + sockfd);
			}
			if( events[i].events & EPOLLOUT ){
				if( (write_ret = mash_write(coredata + sockfd)) < 0 )
					mash_close(coredata + sockfd);
				else if(write_ret == 0)
					modevent(epollfd, sockfd, EPOLLIN|EPOLLOUT);
				else 
					modevent(epollfd, sockfd, EPOLLIN);
			}
		}//end epoll event scan.
	}
	close(epollfd);
	close(listenfd);
	exit(1);
}
