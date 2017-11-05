#ifndef UTILS
#define UTILS
#define TRUE 1
#define FALSE 0
#define CMD_ERR_TOO_LONG 0
#define CMD_ERR_CLOSED 1
#define CMD_ERR_TLE 2
#define CMD_ERR_UNKNOWN 3
#define ERRNO_TLE 11
#include"user.h"
#include<arpa/inet.h>

extern const char* msg_hello;
extern const char* msg_cmd_too_long;
extern const char* msg_invalid_syntax;
extern const char* msg_system;
extern const char* msg_type_ok;
extern const char* msg_type_failed;
extern const char* msg_quit;
extern const char* msg_user_failed;
extern const char* msg_user_ok;
extern const char* msg_pass_no_user;
extern const char* msg_pass_ok;
extern const char* msg_pass_failed;
extern const char* msg_already_logged;
extern const char* msg_login_required;
extern const char* msg_port_illegal;
extern const char* msg_port_success;
extern const char* msg_pasv_failed;
extern const char* msg_connection_required;
extern const char* msg_path_illegal;
extern const char* msg_open_file_failed;
extern const char* msg_no_data_connection;
extern const char* msg_connection_crash;
extern const char* msg_file_transer_ok;
extern const char* msg_file_create_failed;
extern const char* msg_stor_prepared;
extern const char* msg_cwd_fail;
extern const char* msg_cwd_ok;
extern const char* msg_list_prepare;
extern const char* msg_mkd_failed;
extern const char* msg_mkd_ok;
extern const char* msg_rmd_failed;
extern const char* msg_rmd_ok;


extern char ip_pc[20];
extern char* root_dir;
extern int init_ip_pc();	//将本机ip填入ip_pc中
extern int safe_command_read(int sock_fd, char* buffer, int size, int* error);	//从已经设置超时的sock_fd中读取command到buffer中，如果成功返回读取的大小，错误返回-1，错误信息记录在error中
extern int parse_command(char* cmd, int size);	//返回cmd中的指令和参数的分界点，如果命令不是以\r\n结尾则返回-1
extern void clear_user(struct user* puser);
extern void init_user(struct user* puser, int sock_visitor);
extern void easy_reply(int sock, const char* reply);
extern int port_addr_check(char* para, struct sockaddr_in* paddr);
extern int get_pasv_sock(unsigned short int *port, int* sock);//成功时返回0，在port和sock中填写监听的端口值和监听用的socket
extern void get_pasv_addr(unsigned short int port, char* buffer);//在buffer中填入本机ip+端口号
extern int simple_find(const char* target, const char* pattern);
extern int path_legacy_check(const char *path);		//路径合法(不含../)返回1，不合法返回0
extern void close_connection(struct user* puser);
extern int safe_write(int sock, const char *buffer, int length);	//return 1 if all bytes wrote successful, otherwise 0
extern char* build_file_path(const struct user* puser);
extern int find_para_pos(int argc, char **argv, char *name);	//总参数个数必须为偶数
extern int is_number(char *str);
#endif
