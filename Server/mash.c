#include	"unp.h"
#include	<stdlib.h>
#include	<stdbool.h>
#include	<sys/epoll.h>
//#include	<string.h>
#include 	<string.h>
#include 	"mash.h"

int setnonblocking( int fd )
{
    int old_option = fcntl( fd, F_GETFL );
    int new_option = old_option | O_NONBLOCK;
    Fcntl( fd, F_SETFL, new_option );
    return old_option;
}

void addevent( int epollfd, int fd, bool one_shot )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if( one_shot ){
        //event.events |= EPOLLONESHOT;
    }
    Epoll_ctl( epollfd, EPOLL_CTL_ADD, fd, &event );
    setnonblocking( fd );
    return;
}

void modevent( int epollfd, int fd, int ev )
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    Epoll_ctl( epollfd, EPOLL_CTL_MOD, fd, &event );
    return;
}

void delevent(int epollfd, int fd)
{
    Epoll_ctl( epollfd, EPOLL_CTL_DEL, fd, 0 );
}

enum MASH_DATA_TYPE mash_type(char *reply)
{
	if(!strncmp(reply, "Mashctl:", 8))
                return MASH_CTL;
	if(!strncmp(reply, "Mashcmd:", 8))
                return MASH_CMD;
	if(!strncmp(reply, "Mashinfo:", 9))
                return MASH_INFO;
	if(!strncmp(reply, "Mashdata:", 9))
                return MASH_DATA;
	return MASH_DATA;
}

void mash_display(struct mashdata *data, int sockfd)
{
	int i = 0;
	int nbytes = 0;
	for(i = 0; i < 10; ++i){
		if(data[i].role == 1){
			memcpy(data[sockfd].reply + nbytes, "Mashdata:", 9);
			nbytes += 9;
			if( data[i].selected == sockfd)
				nbytes += snprintf(data[sockfd].reply + nbytes, 3, "++");
			else if( data[i].selected == 0)
				nbytes += snprintf(data[sockfd].reply + nbytes, 3, "--");
			else
				nbytes += snprintf(data[sockfd].reply + nbytes, 3, "+-");
			nbytes += snprintf(data[sockfd].reply + nbytes, 1024,\
				"id: %d, role: client, addess: %s @%s\n", i, data[i].client_pri, data[i].client_pub);
		}//end if role 
	}
	Write(sockfd, data[sockfd].reply, nbytes);
}

void mash_show(struct mashdata *data)
{
	int i;
	for(i = 0; i < 10; ++i){
		if( data[i].selected){
			printf("%s",data[i].reply);
		}
	}
	return;
}

int mash_select(struct mashdata *data, int id, int sockfd)
{
	int nbytes = 0;
	int admin_id = sockfd;
	if( data[id].selected > 0 && data[id].selected != admin_id){
		nbytes += snprintf(data[admin_id].reply + nbytes, 1024, "Mashdata:Has selected by another admin!\n");
	}else if(data[id].role == 1 && data[admin_id].role == 9){
		data[id].selected = admin_id;
		nbytes += snprintf(data[admin_id].reply + nbytes, 1024, "Mashdata:Select slave: %d ok!\n", id);
	}
	Write(sockfd, data[admin_id].reply, nbytes);
	return 0;
}

int mash_unselect(struct mashdata *data, int n, int admin_id)
{
	if(data[n].role > 0 && data[n].selected == admin_id)
		data[n].selected = 0;
}

int selected_num(struct mashdata *data, int admin_id)
{
	int id;
	int count = 0;
	for(id = 0; id < MAX_CLIENT_NUM; ++id){
		if( data[id].selected == admin_id){
			++count;
		}
	}
	return count;
}

void mash_help(struct mashdata *data, int sockfd)
{
	int nbytes = 0;
	nbytes += snprintf(data[sockfd].reply + nbytes, 10, "Mashdata:");
	nbytes += snprintf(data[sockfd].reply + nbytes, 1024, "\n \
	help info: \n \
	mashcmd  	basic command mode.\n \
	mashcli  	command mode, sent ctl to all the selected slave.\n \
	interface  	inter interactive mode with the selected slave.\n \
	select  	choose one slave.\n \
	unselect  	unselect one slave.\n \
	display  	show client list.\n \
	show  		show all the client run result.\n \
	help  		show this page.\n\n"
	); 

	Write(sockfd, data[sockfd].reply, nbytes);
}

int mash_auth(struct mashdata *data)
{
	char login_ip[] = "127.0.0.1";
	if(data->role == 1)	/* Already login */
		return 1;
	if(data->role == 9)	/* Already login */
		return 9;
	/* Is admin */
	if(!strncmp( data->client_pub, login_ip, 9)){
		data->role = 9;
		data->status = CMD;
		return 9;
	}else{
		data->role = 1;
                data->status = STANDBY;
                return 1;
	}
	return 0;
}

int mash_init(struct mashdata *coredata, int sockfd, struct sockaddr_in client_addr)
{
	int auth_ret = 0;
	struct mashdata *data = coredata + sockfd;
	memset(data, '\0', sizeof(struct mashdata));
	setnonblocking(sockfd);
	data->connfd = sockfd;
	data->selected = 0;
	data->nrequest = 0;
	data->nreply = 0;
	memset(data->request, '\0', REPLY_SIZE);
	memset(data->reply, '\0', REPLY_SIZE);
	memcpy(data->client_pub, inet_ntoa(client_addr.sin_addr), 16);
	auth_ret = mash_auth(data);
	if( 1 == auth_ret ){
		data->role = 1;
                data->status = STANDBY;
	}else if( 9 == auth_ret ){
		data->role = 9;
                data->status = CMD;
	}else{
		mash_close(coredata, sockfd);
                return -1;
	}
	return 0;
}

int mash_data(struct mashdata *all_data, int sock_fd, int epollfd)
{
	int id = 0;
	int selected_count = 0;
	int admin_id = 0;
	int nbytes = 0;
	int data_size = all_data[sock_fd].nreply - 9;
	char *data = all_data[sock_fd].reply + 9;

	/* If data from slaves */
	if(all_data[sock_fd].role == 1){
		if( all_data[sock_fd].selected  <= 0  )
			return 1;
		admin_id = all_data[sock_fd].selected;
		/* return reply to admin console. */
		if( 9 == all_data[admin_id].role && all_data[admin_id].status == INTERFACE ){
			memcpy(data - 9, "Mashdata:", 9);
			nbytes = Write(all_data[admin_id].connfd, data - 9, data_size + 9);
		}
		return nbytes;
	}

	/* If data from controls */
	if(all_data[sock_fd].role == 9){
		/* On CLI status sent command to client. */
		if(CLI == all_data[sock_fd].status | \
				INTERFACE == all_data[sock_fd].status){
			for(id = 0; id < MAX_CLIENT_NUM; ++id){
				selected_count ++;
				if( all_data[id].selected == sock_fd \
					& WORK == all_data[id].status ){
					memcpy(all_data[id].reply, "Mashdata:", 10);
					all_data[id].nreply = data_size + 9;
					memcpy(all_data[id].reply + 9, data, data_size);
					modevent(epollfd, id, EPOLLOUT);
				}
			}
			if( 0 == selected_count ){
				all_data[sock_fd].status = CMD;
				write(sock_fd, "Mashctl:mashcmd!", 16);
			}
		}else if( CMD == all_data[sock_fd].status ){
			all_data[sock_fd].status = CMD;
			write(sock_fd, "Mashctl:mashcmd!", 16);
		}
	}
	return 3;
}

int mash_ctl(struct mashdata *data, int sock_fd, int epollfd)
{
	int id = 0;
	int nbytes = 0;
	int admin_id = 0;
	char *ctl = data[sock_fd].reply + 8;
	/* If ctl from slaves */
	if( 1 == data[sock_fd].role ){
		if(!strncmp(ctl, "work!", 5)){
			data[sock_fd].status = WORK;
			admin_id = data[sock_fd].selected;
			if( admin_id ){
				data[admin_id].status = INTERFACE;
				write(admin_id, "Mashctl:interface!", 18);
			}
		}
		if(!strncmp(ctl, "standby!", 8)){
			data[sock_fd].status = STANDBY;
			admin_id = data[sock_fd].selected;
			if( admin_id ){
				data[admin_id].status = CMD;
				write(admin_id, "Mashctl:mashcmd!", 16);
			}
		}
		return 1;
	}
	
	/* For role is not 9. */
	if(9 == data[sock_fd].role){
		/* Loop search for the selected client */
		for(id = 0; id < MAX_CLIENT_NUM; ++id){
			if( data[id].selected == sock_fd ){
				nbytes = snprintf(data[id].reply, 1024, \
					"Mashctl:work!");
				data[id].nreply = nbytes;
				modevent(epollfd, id, EPOLLOUT);
				break;
			}
       		}
		return 9;
	}

	return 0;
}

int mash_cmd(struct mashdata *data, int sock_fd, int epollfd)
{
	int id = 0;
	int admin_id = 0;
	int nbytes = 0;
	char *cmd = data[sock_fd].reply + 8;

	/* check mashctl command */
	if(!strncmp(cmd, "mashctl", 7)){
		data[sock_fd].status = CMD;
		write(sock_fd, "Mashctl:mashctl!", 16);
		return 9;
	}
	if(!strncmp(cmd, "mashcli", 7)){
		if( selected_num( data, sock_fd) < 1){
			nbytes = snprintf(data[sock_fd].reply, 1024,\
				"Error: select at least one interface.\r\n");
			Write(sock_fd, data[sock_fd].reply, nbytes);
		}else
			data[sock_fd].status = CLI;
		return 9;
	}
	if(!strncmp(cmd, "interface", 9)){
		if( selected_num( data , sock_fd) != 1){
			nbytes = snprintf(data[sock_fd].reply, 1024, \
				"Error: Please choose only one interface.\r\n");
			Write(sock_fd, data[sock_fd].reply, nbytes);
		}else{
			mash_ctl(data, sock_fd, epollfd);
		}
		return 9;
	}
	if(!strncmp(cmd, "display", 7)){
		mash_display(data, sock_fd);
		return 9;
	}
	if(!strncmp(cmd, "show", 4)){
		mash_show(data);
		write(sock_fd, "show...", 7);
		return 9;
	}
	if(!strncmp(cmd, "select", 6)){
		mash_select(data, atoi(cmd + 6), sock_fd);
		return 9;
	}
	if(!strncmp(cmd, "unselect", 8)){
		mash_unselect(data, atoi(cmd + 8), sock_fd);
		return 9;
	}
	if(!strncmp(cmd, "help", 4)){
		mash_help(data, sock_fd);
		return 9;
	}
	if(!strncmp(cmd, "\4", 1)){
		mash_close(data, sock_fd);
		return 9;
	}

	if(!strncmp(cmd, "\n", 1)){
		return 9;
	}else{
		/* unknow ctl info sent to controls */
		nbytes = snprintf(data[sock_fd].reply, 1024,\
			"Error: unknow mash ctl!.\r\n");
		Write(sock_fd, data[sock_fd].reply, nbytes);
	}
	return 0;
}

int mash_info(struct mashdata *data, int sockfd)
{
	printf("%s\n", data[sockfd].reply);
        memcpy(data[sockfd].client_pri, data[sockfd].reply + 10, 16);
}

int mash_process(struct mashdata *data, int sockfd, int epollfd)
{
	int admin_id = 0;
	/* Check Magic number from data.reply */
	switch( mash_type(data[sockfd].reply) ){
		case (MASH_CMD):
			if( 9 == mash_cmd(data, sockfd, epollfd))
				Write(sockfd, "Mashdata:[mashctl]%", 19);
			break;
		case (MASH_CTL):
			if( 9 == mash_ctl(data, sockfd, epollfd))
				Write(sockfd, "Mashdata:[mashctl]%", 19);
			break;
		case (MASH_INFO):
			mash_info(data, sockfd);
			break;
		case (MASH_DATA):
			mash_data(data, sockfd, epollfd);
			break;
		default:
			printf("unknow client");
			mash_close(data, sockfd);
	}
	return 0;
}

int mash_read(struct mashdata *data, int sockfd)
{
	int nread;
        memset(data[sockfd].reply, '\0', REPLY_SIZE);
	if ( (nread = read(sockfd, data[sockfd].reply, REPLY_SIZE)) == 0){
		printf("Connection closed by other end");
		return 0;
	}
	data[sockfd].nreply = nread;
	return nread;
}

int mash_write(struct mashdata *data, int sockfd)
{
	return Write(sockfd, data[sockfd].reply, data[sockfd].nreply);
}

int mash_close(struct mashdata *data, int sockfd)
{
	int i;
	int admin_id = 0;
	if(data[sockfd].role == 1){
		admin_id = data[sockfd].selected;
		if( admin_id ){
			data[admin_id].status = CMD;
			Write(admin_id, "Mashnote:client closed. \n", 24);
			Write(admin_id, "Mashctl:mashctl!", 16);
		}
	}else if( data[sockfd].role == 9 ){
		admin_id = sockfd;
		for(i = 0; i < 10; ++i){
			if(data[i].role == 1 && data[i].selected == admin_id){
				data[i].selected = 0;
				data[i].status = CMD;
                	}
        	}
	}
	data[sockfd].role = 0;
	data[sockfd].connfd = -1;
	data[sockfd].selected = 0;

	close(sockfd);
	return 0;
}

