#include"utils.h"
#include<unistd.h>
#include<errno.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<memory.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<time.h>

#define RANDOM_PORT rand()%(65535-20000)+20000
char ip_pc[20];
const char *msg_hello = "220 Liangjz's server ready.\r\n";
const char *msg_cmd_too_long = "500 command to long.\r\n";
const char *msg_invalid_syntax = "500 invalid syntax.\r\n";
const char *msg_system = "215 UNIX Type: L8\r\n";
const char *msg_type_ok = "200 Type set to I.\r\n";
const char *msg_type_failed = "500 Type setting failed.\r\n";
const char *msg_quit = "221 Goodbye.\r\n";
const char *msg_user_ok = "331 Please specify password.\r\n";
const char *msg_user_failed = "530 Can not change from guest user.\r\n";
const char *msg_pass_no_user = "503 Login with USER first.\r\n";
const char *msg_pass_ok = "230 Login successful.\r\n";
const char *msg_pass_failed = "530 Login incorrect.\r\n";
const char *msg_already_logged = "230 Already logged in.\r\n";
const char *msg_login_required = "530 Please login with USER and PASS.\r\n";
const char *msg_port_success = "200 PORT command successful.\r\n";
const char *msg_port_illegal = "500 Illegal PORT command.\r\n";
const char *msg_pasv_failed = "530 PASV failed.\r\n";
const char *msg_connection_required = "425 Use PORT or PASV first.\r\n";
const char *msg_path_illegal = "530 Path shall not contain\"../\".\r\n";
const char *msg_open_file_failed = "551 Failed to open file.\r\n";
const char *msg_no_data_connection = "425 Failed to establish connection.\r\n";
const char *msg_connection_crash = "426 Data connection broken.\r\n";
const char *msg_file_transer_ok = "226 File transfer ok.\r\n";
const char *msg_file_create_failed = "552 Failed to create file\r\n";
const char *msg_stor_prepared = "150 OK to send data.\r\n";
const char *msg_cwd_fail = "550 Failed to change directory.\r\n";
const char *msg_cwd_ok = "250 Directory successfully changed.\r\n";
const char *msg_list_prepare = "150 Here comes directory listing.\r\n";
const char *msg_mkd_failed = "550 Failed to create directory.\r\n";
const char *msg_mkd_ok = "250 Successfully created directory.\r\n";
const char *msg_rmd_ok = "250 Successfully removed directory.\r\n";
const char *msg_rmd_failed = "550 Failed to remove directory.\r\n";

int safe_command_read(int sock_fd, char* buffer, int size, int* error)
{
	int p = 0, count;
	while(TRUE)
	{
		count = read(sock_fd, buffer + p, size);
		if(count > 0)
		{
			//correctly read
			p += count;
			if(p > size)
			{*error = CMD_ERR_TOO_LONG;return -1;}
			if(buffer[p - 1] == '\n')
			{break;}
			continue;
		}
		if(count == 0)
		{*error = CMD_ERR_CLOSED;return -1;}
		if(count == -1 && errno == ERRNO_TLE)
		{*error = CMD_ERR_TLE;return -1;}
		//unkonw error
		*error = 3;
		return -1;
	}
	return p;
}

int parse_command(char* cmd, int size)
{
	if(size <=2 || !(cmd[size - 1] == '\n' && cmd[size - 2] == '\r'))
		return -1;
	int i;
	for(i = 0; i < size - 2; ++i)
	{
		if(cmd[i] == ' ')
			break;
	}
	return i;
}
void clear_user(struct user* puser)
{
	puser->is_active = FALSE;
	if(puser->connect_status == CONNECT_PASV)
		close(puser->sock_data);
	close(puser->sock_command);
	free(puser->current_path.data);
	memset(puser, 0, sizeof(*puser));
}

void init_user(struct user* puser, int sock_visitor)
{
	struct timeval timeout;
	timeout.tv_sec = READ_TIME_OUT;
	timeout.tv_usec = 0;
	puser->is_active = TRUE;
	puser->sock_command = sock_visitor;
	setsockopt(sock_visitor, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	puser->current_path.length = 512;
	puser->current_path.data = malloc(puser->current_path.length * sizeof(char));
	strcpy(puser->current_path.data, "/");
	easy_reply(puser->sock_command, msg_hello);
}

void easy_reply(int sock, const char* reply)
{
	int count_bytes = strlen(reply), bytes_wrote = 0;
	while(bytes_wrote < count_bytes)
	{
		bytes_wrote = write(sock, reply + bytes_wrote, count_bytes - bytes_wrote);
		if(bytes_wrote <= 0)
			return;
	}
}

int safe_write(int sock, const char *buffer, int length)
{
	int p = 0, count = 0;
	while(p < length)
	{
		count = write(sock, buffer + p, length - p);
		if(count <= 0)
			return FALSE;
		p += count;
	}
	return TRUE;
}

int port_number_check(char* s, int len)
{
	if(len == 0 || len > 3)
		return -1;
	s[len] = '\0';
	return atoi(s);
}

int port_addr_check(char* para, struct sockaddr_in* paddr)
{
	int sep_pos[5], len = strlen(para), i, count_sep = 0;
	//计算分隔符位置，有且只应有5个分隔符
	for(i = 0; i < len; ++i)
	{
		if(!(para[i] >= '0' && para[i] <= '9'))
		{
			if(count_sep == 5)
				return -1;
			sep_pos[count_sep] = i;
			++count_sep;
		}
	}
	if(count_sep < 5)
		return -1;
	for(i = 0; i < 5; ++i)
	{
		para[sep_pos[i]] = '.';
	}
	//获取地址
	paddr->sin_family = AF_INET;
	para[sep_pos[3]] = '\0';
	if(inet_pton(AF_INET, para, &paddr->sin_addr) <= 0)
		return -1;
	unsigned short int p1, p2;
	p1 = port_number_check(para + sep_pos[3] + 1, sep_pos[4] - sep_pos[3] -1);
	p2 = port_number_check(para + sep_pos[4] + 1, len - sep_pos[4] -1);
	paddr->sin_port = htons(p1 * 256 + p2);
	return 0;
}
int get_pasv_sock(unsigned short int *port, int* sock)
{
	const int max_loop = 500;
	//routines creating listening socket
	int listenfd;
	struct sockaddr_in addr_listen;
	listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset(&addr_listen, 0, sizeof(addr_listen));
	addr_listen.sin_family = AF_INET;

	unsigned short int port_listen;
	int bind_ok, i = 0;
	do
	{
		port_listen = RANDOM_PORT;
		addr_listen.sin_port = htons(port_listen);
		bind_ok = bind(listenfd, (struct sockaddr*)&addr_listen, sizeof(addr_listen));
		++i;
	}while(i <= max_loop && bind_ok == -1);
	if(bind_ok == -1)
	{
		close(listenfd);
		return -1;
	}
	listen(listenfd, 2);
	//设置transfer_sock的阻塞时间
	struct timeval timeout;
	timeout.tv_sec = READ_TIME_OUT_DATA;
	timeout.tv_usec = 0;
	setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	//fcntl(listenfd, F_SETFL, O_NONBLOCK);	//setting sockets to be nonblocking so that accept can return error
	*port = port_listen; *sock = listenfd;
	return 0;
}

int init_ip_pc()
{
	int res_bash = system("ifconfig | grep inet[^6] | awk -F \" \" \'{if(NR != 1){print $2}}\' > ip.txt");
	if(res_bash != 0)
	{
		printf("error bash\n");
		return -1;
	}
	int fd = open("ip.txt", O_RDONLY);
	if(fd == -1)
	{
		printf("error open ip.txt");
		return -1;
	}
	int count_read = read(fd, ip_pc, sizeof(ip_pc));
	ip_pc[count_read - 1] = '\0';	//从文件中读取的串自带回车
	system("rm -f ip.txt");
	return 0;
}

void get_pasv_addr(unsigned short int port, char* buffer)
{
	strcpy(buffer, ip_pc);
	int len;
	len = strlen(buffer);
	sprintf(buffer + len, ".%d", port / 256);
	len = strlen(buffer);
	sprintf(buffer + len, ".%d", port % 256);

	//测试用
	int i;
	len = strlen(buffer);
	for(i = 0; i < len; ++i)
	{
		if(!(buffer[i] >= '0' && buffer[i] <= '9'))
		{
			buffer[i] = ',';
		}
	}
}

int simple_find(const char* target, const char* pattern)
{
	int l_tar = strlen(target), l_pat = strlen(pattern);
	if(l_tar < l_pat)
		return -1;
	int i, j;
	for(i = 0; i < l_tar; ++i)
	{
		for(j = 0; j < l_pat; ++j)
		{
			if(target[i + j] != pattern[j])
				break;
		}
		if(j == l_pat)
			return i;
	}
	return -1;
}
//合法返回1， 不合法返回0
int path_legacy_check(const char *path)
{
	if(path[0] == '/')
	{
		path += 1;
	}
	if(strcmp(path, "..") == 0)
		return FALSE;
	return simple_find(path, "../") < 0;
}

//在传输文件完成或失败时用以关闭
void close_connection(struct user* puser)
{
	//对于port命令的数据传输，应当自己申请自己销毁
	if(puser->connect_status == CONNECT_PASV)
	{
		close(puser->sock_data);
	}
	puser->connect_status = CONNECT_NOT_EXIST;
}

char *build_file_path(const struct user* puser)
{
	//构造用户文件绝对路径，分用户输入的是相对路径还是绝对路径做不同的处理
	char *path_abs = NULL;
	int len_path_abs = 0;
	if(puser->command_block[0] == '/')
	{
		//用户输入绝对路径
		len_path_abs = strlen(root_dir) + strlen(puser->command_block) + 2;
		path_abs = malloc(sizeof(char) * len_path_abs);
		strcpy(path_abs, root_dir);
		strcat(path_abs, puser->command_block);
	}
	else
	{
		//用户输入相对路径
		len_path_abs = strlen(root_dir) + strlen(puser->current_path.data) + strlen(puser->command_block) + 2;
		path_abs = malloc(sizeof(char) * len_path_abs);
		strcpy(path_abs, root_dir);
		strcat(path_abs, puser->current_path.data);
		strcat(path_abs, puser->command_block);
	}
	return path_abs;
}
void update_user_path(struct str_path *p_user_path, char *path)
{
	int len_path = strlen(path);
	if(p_user_path->length <= len_path)
	{
		free(p_user_path->data);
		p_user_path->length = 2 * len_path;
		p_user_path->data = (char*) malloc(sizeof(char) * p_user_path->length);
	}
	strcpy(p_user_path->data, path);
}
void add_user_path(struct str_path *p_user_path, char *path)
{
	int len_path = strlen(p_user_path->data) + strlen(path);
	char *tmp = NULL;
	if(len_path >= p_user_path->length)
	{
		tmp = (char*) malloc(sizeof(char) * len_path * 2);
		strcpy(tmp, p_user_path->data);
		free(p_user_path->data);
		p_user_path->data = tmp;
		p_user_path->length = 2 * len_path;
	}
	strcat(p_user_path->data, path);
}
int find_para_pos(int argc, char **argv, char* name)
{
	int i = 0;
	for(; i < argc; i +=2)
	{
		if(simple_find(argv[i], name) != -1)
		{
			return i + 1;
		}
	}
	return -1;
}
int is_number(char *str)
{
	int len = strlen(str);
	int i;
	for(i = 0; i < len; ++i)
	{
		if(!(str[i] >= '0' && str[i] <= '9'))
			return FALSE;
	}
	return TRUE;
}
