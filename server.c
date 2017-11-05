#include <netinet/in.h>
#include<stdlib.h>
#include<time.h>
#include <unistd.h>
#include <errno.h>
#include<fcntl.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include<sys/time.h>
#include<sys/select.h>
#include <sys/socket.h>
#include<sys/types.h>
#include"user.h"
#include"utils.h"
#include"processors.h"
#include"workers.h"
void pre_quit(int sig);
void reject_visitor(int sock);
int process_command(struct user* puser);	//return 0 if success, otherwise -1
int init_from_cmd(int argc, char **argv, char **root, unsigned short *port, int *count_worker);
struct user users[COUNT_USER];	//存放user的槽，本ftp服务器最多支持1000个user同时在线，多于此数目将会自动拒绝其他的连接请求,出于性能考虑，这个变量不加锁
/*
const char *cmd_user = "USER", *cmd_pass = "PASS", *cmd_retr = "RETR", *cmd_stor = "STOR", *cmd_quit = "QUIT", *cmd_syst = "SYST", *cmd_type = "TYPE", *cmd_port = "PORT", *cmd_pasv = "PASV", *cmd_mkd = "MKD", *cmd_cwd = "CWD", *cmd_list = "LIST", *cmd_rmd = "RMD";
*/
const char *list_cmd[] = {"USER", "PASS", "RETR", "STOR", "QUIT", "SYST", "TYPE", "PORT", "PASV", "MKD", "CWD", "LIST", "RMD"};
typedef int (*cmd_processor)(struct user* puser, char* para);
cmd_processor processor_lst[] = {process_user, process_pass, process_retr, process_stor, process_quit, process_syst, process_type, process_port, process_pasv, process_mkd, process_cwd, process_list, process_rmd};

char *root_dir = NULL;
char *root_defalut = "/tmp";	//TODO:根据命令行输入设定root
int main(int argc, char **argv)
{
	int count_worker = 2;		//TODO:根据命令行输入确定worker数量
	unsigned short int port_listen = 21;		//TODO:根据输入的参数调整监听的端口号
	root_dir = root_defalut;
	argc -= 1; argv += 1;	//忽略名称参数
	if(init_from_cmd(argc, argv, &root_dir, &port_listen, &count_worker) != 0)
	{
		return 1;
	}
	//初始化工作
	int listenfd;
	struct sockaddr_in addr_listen;
	if ((listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error creating listening socket: %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	memset(&addr_listen, 0, sizeof(addr_listen));
	addr_listen.sin_family = AF_INET;
	addr_listen.sin_port = htons(port_listen);
	addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(listenfd, (struct sockaddr*)&addr_listen, sizeof(addr_listen)) == -1) {
		printf("Error bind listening socket: %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	if(listen(listenfd, 10) == -1)
	{
		printf("Error start listening: %s(%d)\n", strerror(errno), errno);
		return 1;
	}
	memset(users, 0, sizeof(*users) * COUNT_USER);
	srand(time(NULL));
	if(init_ip_pc() == -1)
	{
		printf("Error getting PC's IP\n");
		return 1;
	}
	worker_init(count_worker);
	fd_set readfds;		//fd_set used by select
	int max_sd, activity, i;	//select 需要一个max_fd, 同时返回activity
	int count_user_accepted = 0;	//已经接收的用户socket数量
	int new_visitor_sock = 0;
	//server 的主工作循环：不断地监听是否有是否有新的socket连接到当前socket或者有已经连接的socket可读
	while(1)
	{
		//clear socket set
		FD_ZERO(&readfds);
		//adding master socket
		FD_SET(listenfd, &readfds);
		max_sd = listenfd;
		//adding user command fd into fd set
		for(i = 0; i < COUNT_USER; ++i)
		{
			//主线程应当始终监听所有的套接字
			if(users[i].is_active)
			{
				FD_SET(users[i].sock_command, &readfds);
				if(users[i].sock_command > max_sd)
				{
					max_sd = users[i].sock_command;
				}
			}
		}
		//waiting for readable signals for infinitly long
		activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
		if(activity < 0)
		{
			printf("select error: %s(%d)\n", strerror(errno), errno);
			return 1;
		}
		if(FD_ISSET(listenfd, &readfds))
		{
			//listenfd is connected
			new_visitor_sock = accept(listenfd, NULL, NULL);
			if(new_visitor_sock == -1)
			{
				printf("error accepting:%s(%d)\n", strerror(errno), errno);
				return 1;
			}
			if(count_user_accepted >= COUNT_USER)
			{
				reject_visitor(new_visitor_sock);
			}
			else
			{
				//accept new vistor
				for(i = 0; i < COUNT_USER; ++i)
				{
					if(!users[i].is_active)
					{
						init_user(&users[i], new_visitor_sock);
						++count_user_accepted;
						break;
					}
				}				
			}
		}
		for(i = 0; i < COUNT_USER; ++i)
		{
			if(users[i].is_active && (FD_ISSET(users[i].sock_command, &readfds)))
			{
				if(process_command(&users[i]) == -1)
				{
					printf("processing error\n");
					return 1;
				}
			}
		}
	}
	return 0;
}

void reject_visitor(int sock)
{
	char *msg = "421 sorry but too much user currently\r\n";
	write(sock, msg, strlen(msg));
	close(sock);
}

int process_command(struct user* puser)
{
	if(puser->is_transmitting)
		return 0;
	const int size_buffer = SIZE_COMMAND;
	char cmd_buffer[size_buffer];
	int error;
	//reading command
	int count_read = safe_command_read(puser->sock_command, cmd_buffer, size_buffer, &error);
	if(count_read == -1)
	{
		switch(error)
		{
			case CMD_ERR_CLOSED:
				//用户已经退出，必须及时关闭这个socket，不然将永远是可读的
				clear_user(puser);
				return 0;
			case CMD_ERR_UNKNOWN:
				clear_user(puser);
				return 0;
			case CMD_ERR_TOO_LONG:
				write(puser->sock_command, msg_cmd_too_long, strlen(msg_cmd_too_long));
				return 0;
			default:
				return 0;
		}
	}
	//parsing command
	int sep = parse_command(cmd_buffer, count_read);
	if(sep == -1)
	{
		write(puser->sock_command, msg_invalid_syntax, strlen(msg_invalid_syntax));
		return 0;
	}
	char para_buffer[size_buffer];
	int i;
	for(i = sep + 1; i < count_read - 2; ++i)
	{
		para_buffer[i - sep - 1] = cmd_buffer[i];
	}
	para_buffer[count_read - sep - 3] = '\0';	cmd_buffer[sep] = '\0';
	//command process
	for(i = 0; i < sizeof(list_cmd) / sizeof(*list_cmd); ++i)
	{
		if(strcmp(list_cmd[i], cmd_buffer) == 0)
		{
			return processor_lst[i](puser, para_buffer);
		}
	}
	return process_default(puser, para_buffer);
}
//如果没有相应参数则 不改变变量的值
int init_from_cmd(int argc, char **argv, char **root, unsigned short *port, int *count_worker)
{
	if(argc % 2 != 0)
	{
		printf("invalid parameter type\n");
		return 1;
	}
	//设定root
	int len_root = 0, tmp;
	char *cmd_root_check = NULL;
	if((tmp = find_para_pos(argc, argv, "root")) != -1)
	{
		len_root = strlen(argv[tmp]);
		cmd_root_check = malloc(sizeof(char) * (len_root + 100));
		sprintf(cmd_root_check, "cd %s 2> errorlog.txt", argv[tmp]);
		if(system(cmd_root_check) != 0)
		{
			printf("invalid path setting\n");
			free(cmd_root_check);
			return 1;
		}
		free(cmd_root_check);
		*root = malloc(sizeof(char) * (len_root + 100));
		strcpy(*root, argv[tmp]);
	}
	//设定端口
	if((tmp = find_para_pos(argc, argv, "port")) != -1)
	{
		if(!is_number(argv[tmp]))
		{
			printf("invalid port number\n");
			return 1;
		}
		*port = (unsigned short int)atoi(argv[tmp]);
	}
	//设定工作线程数
	if((tmp = find_para_pos(argc, argv, "worker")) != -1)
	{
		if(!is_number(argv[tmp]))
		{
			printf("invalid worker number");
			return 1;
		}
		*count_worker = atoi(argv[tmp]);
	}
	return 0;
}
