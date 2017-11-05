#ifndef USER
#define USER
#include<netinet/in.h>

#define USER_NOT_LOGGED 0
#define USER_PASS_REQUIRED 1
#define USER_LOGGED 2
#define CONNECT_NOT_EXIST 0
#define CONNECT_PORT 1
#define CONNECT_PASV 3

#define SIZE_COMMAND 1024

#define BLOCK_OP_RETR 0
#define BLOCK_OP_STOR 1
#define BLOCK_UP_LIST 2
struct str_path
{
	int length;
	char *data;
};
struct user
{
	char is_active;
	int sock_command;	//监听用户命令的socket描述符
	int log_status;		//0, 1, 2分别表示未登录，已登录但未输密码，已登录
	int is_annonymous;	//拒绝一切非annonymous用户
	struct sockaddr_in port_cmd_addr;	//保存port命令的地址
	int sock_data;		//保留了用户使用PASV命令后监听用的socket
	int connect_status;
	char is_transmitting;
	int block_operation_type;
	char command_block[SIZE_COMMAND + 10];	//当前不支持任意长命令，由于命令处理线程是单线程的，容许任意长度的命令可能导致读入命令时间过长而导致其他命令服务饥饿；此外，定长的命令存储可以有效地避免内存泄露
	struct str_path current_path;	//用户的当前路径始终以'/'结尾
};
#define COUNT_USER 1000
#define READ_TIME_OUT 1
#define READ_TIME_OUT_DATA 10
extern struct user users[COUNT_USER];
extern void update_user_path(struct str_path* p_user_path, char* path);
extern void add_user_path(struct str_path *p_user_path, char* path);
#endif
