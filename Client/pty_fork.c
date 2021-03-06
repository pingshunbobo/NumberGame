#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

int ptym_open(char *pts_name, int pts_namesz)
{
	int fdm;

	strncpy(pts_name, "/dev/ptmx", pts_namesz);
	pts_name[pts_namesz - 1] = '\0';

	fdm = posix_openpt(O_RDWR);
	if(fdm < 0)
		return -1;
	if(grantpt(fdm) < 0){
		close(fdm);
		return -2;
	}
	if(unlockpt(fdm) < 0){
		close(fdm);
		return -3;
	}
	if( ptsname_r(fdm,pts_name,pts_namesz) ){
		close(fdm);
		return -4;
	}
	return fdm;
}

int ptys_open(char* pts_name)
{
	int fds;
	if((fds = open(pts_name, O_RDWR)) < 0)
		return -5;
	return fds;
}

pid_t pty_fork(int *ptyfdm, char *slave_name, int slave_namesize,
			const struct termios *slave_termios,
			const struct winsize *slave_winsize)
{
	int fdm, fds;
	pid_t pid;
	char pts_name[20];

	if((fdm = ptym_open(pts_name, sizeof(pts_name))) < 0)
		log_client("pty_fork: Can`t open master pty : %s", pts_name);
	if(slave_name != NULL){
		strncpy(slave_name, pts_name, slave_namesize);
		slave_name[slave_namesize - 1] = '\0';
	}

	if((pid = fork()) < 0){
		return -1;
	}else if( pid == 0 ){
		if(setsid() < 0)
			log_client("pty_fork: setsid error\n");

		if((fds = ptys_open(pts_name)) < 0)
			log_client("pty_fork: can`t open slave pty\n");
		/* Close the fdm file descriper in the child process. */
		close(fdm);

		if(slave_termios != NULL){
			if(tcsetattr(fds, TCSANOW, slave_termios) < 0)
				log_client("tcsetattr error on slave pty\n");
		}
		if(slave_winsize != NULL){
			if(ioctl(fds, TIOCSWINSZ, slave_winsize) < 0)
				log_client("TIOCSWINSZ error on slave pty\n");
		}
		if(dup2(fds, STDIN_FILENO) != STDIN_FILENO)
			log_client("pty_fork: dup2 error to stdin!\n");
		if(dup2(fds, STDOUT_FILENO) != STDIN_FILENO)
			log_client("pty_fork: dup2 error to stdout!\n");
		if(dup2(fds, STDERR_FILENO) != STDIN_FILENO)
			log_client("pty_fork: dup2 error to stderr!\n");
		if(fds != STDIN_FILENO && fds != STDIN_FILENO && fds != STDIN_FILENO)
			return 0;
	}else{
		*ptyfdm = fdm;
		return pid;
	}
}
