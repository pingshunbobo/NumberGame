/* include serv09 */
#include	"unpthread.h"
#include	"pthread.h"
#include <sys/types.h>
#include <sys/socket.h>

int
main(int argc, char **argv)
{
	int		i;
	int listenfd;
	void	sig_int(int), thread_make(int);

	if (argc == 3)
		listenfd = Tcp_listen(NULL, argv[1], &addrlen);
	else if (argc == 4)
		listenfd = Tcp_listen(argv[1], argv[2], &addrlen);
	else
		err_quit("usage: serv09 [ <host> ] <port#> <#threads>");
	nthreads = atoi(argv[argc-1]);
	tptr = Calloc(nthreads, sizeof(Thread));

	for (i = 0; i < nthreads; i++)
		thread_make(i);			/* only main thread returns */

	Signal(SIGINT, sig_int);

	for ( ; ; )
		pause();	/* everything done by threads */
}
/* end serv09 */

void
sig_int(int signo)
{
	int		i;
	void	pr_cpu_time(void);

	pr_cpu_time();

	for (i = 0; i < nthreads; i++)
		printf("thread %d, %ld connections\n", i, tptr[i].thread_count);

	exit(0);
}
