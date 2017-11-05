#include"processors.h"
#include"user.h"
#include"utils.h"
#include"workers.h"
#include<sys/types.h>
#include<sys/stat.h>
#include<string.h>
#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
int log_check(struct user* puser)
{
	if(puser->log_status != USER_LOGGED)
	{
		easy_reply(puser->sock_command, msg_login_required);
		return FALSE;
	}
	return TRUE;
}

int connection_check(struct user* puser)
{
	if(puser->connect_status == CONNECT_NOT_EXIST)
	{
		easy_reply(puser->sock_command, msg_connection_required);
		return FALSE;
	}
	return TRUE;
}

int process_user(struct user* puser, char* para)
{
	if(puser->log_status == USER_LOGGED)
	{
		easy_reply(puser->sock_command, msg_user_failed);
		return 0;
	}
	//用户未登录或只输入了用户而未输入密码
	if(strcmp(para, "anonymous") == 0)
		puser->is_annonymous = TRUE;
	else
		puser->is_annonymous = FALSE;
	easy_reply(puser->sock_command, msg_user_ok);
	puser->log_status = USER_PASS_REQUIRED;
	return 0;
}
int process_pass(struct user* puser, char* para)
{
	if(puser->log_status == USER_NOT_LOGGED)
	{
		easy_reply(puser->sock_command, msg_pass_no_user);
		return 0;
	}
	else if(puser->log_status == USER_PASS_REQUIRED)
	{
		if(puser->is_annonymous)
		{
			//登录成功
			easy_reply(puser->sock_command, msg_pass_ok); 
			puser->log_status = USER_LOGGED;
		}
		else
		{
			easy_reply(puser->sock_command, msg_pass_failed);
		}
	}
	else
	{
		//用户已经登录
		easy_reply(puser->sock_command, msg_already_logged);
	}
	return 0;
}

int process_block_operation(struct user* puser, char* para, int type_op)
{
	if(!log_check(puser))
		return 0;
	if(!connection_check(puser))
		return 0;
	puser->is_transmitting = TRUE;
	puser->block_operation_type = type_op;
	strcpy(puser->command_block, para);
	push_request(puser);
	return 0;
}

int process_retr(struct user* puser, char* para)
{
	return process_block_operation(puser, para, BLOCK_OP_RETR);
}
int process_stor(struct user* puser, char* para)
{
	return process_block_operation(puser, para, BLOCK_OP_STOR);
}
int process_quit(struct user* puser, char* para)
{
	easy_reply(puser->sock_command, msg_quit);
	clear_user(puser);
	return 0;
}
int process_syst(struct user* puser, char* para)
{
	easy_reply(puser->sock_command, msg_system);
	return 0;
}
int process_type(struct user* puser, char* para)
{
	if(strcmp(para, "I") == 0)
	{
		easy_reply(puser->sock_command, msg_type_ok);
	}
	else
	{
		easy_reply(puser->sock_command, msg_type_failed);
	}
	return 0;
}
int process_port(struct user* puser, char* para)
{
	if(!log_check(puser))
		return 0;
	//如果用户已经使用了PASV，则关闭之前打开的进行监听的socket
	if(puser->connect_status == CONNECT_PASV)
	{
		close(puser->sock_data);
	}
	if(port_addr_check(para, &puser->port_cmd_addr) == -1)
	{
		//连接失败
		easy_reply(puser->sock_command, msg_port_illegal);
		puser->connect_status = CONNECT_NOT_EXIST;
		return 0;
	}
	//连接成功,真正创建socket并尝试连接在工作线程中进行
	easy_reply(puser->sock_command, msg_port_success);
	puser->connect_status = CONNECT_PORT;
	puser->sock_data = -1;
	return 0;
}
int process_pasv(struct user* puser, char* para)
{
	if(!log_check(puser))
		return 0;
	unsigned short int port = 0;
	if(puser->connect_status == CONNECT_PASV)
	{
		close(puser->sock_data);
	}
	if(get_pasv_sock(&port, &puser->sock_data) == -1)
	{
		easy_reply(puser->sock_command, msg_pasv_failed);
		puser->connect_status = CONNECT_NOT_EXIST;
	}
	char buffer_addr[30];
	get_pasv_addr(port, buffer_addr);
	char buffer_reply[100];
	sprintf(buffer_reply, "227 Entering Passive Mode (%s)\r\n", buffer_addr);
	//strcat(buffer_reply, buffer_addr);
	//strcat(buffer_reply, "\r\n");
	easy_reply(puser->sock_command, buffer_reply);
	puser->connect_status = CONNECT_PASV;
	return 0;
}
int process_mkd(struct user* puser, char* para)
{
	if(!log_check(puser))
		return 0;
	if(!path_legacy_check(para))
	{
		easy_reply(puser->sock_command, msg_path_illegal);
		return 0;
	}
	strcpy(puser->command_block, para);
	char *path_abs = build_file_path(puser);
	int status = mkdir(path_abs, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if(status == 0)
		easy_reply(puser->sock_command, msg_mkd_ok);
	else
		easy_reply(puser->sock_command, msg_mkd_failed);
	free(path_abs);
	return 0;
}
int process_cwd(struct user* puser, char* para)
{
	if(!log_check(puser))
		return 0;
	if(!path_legacy_check(para))
	{
		easy_reply(puser->sock_command, msg_path_illegal);
		return 0;
	}
	strcpy(puser->command_block, para);
	char *path_abs = build_file_path(puser);
	char *command = malloc(sizeof(char) * strlen(path_abs) + 128);
	sprintf(command, "cd %s 2>> errolog.txt", path_abs);
	int result = system(command);
	if(result != 0)
	{
		easy_reply(puser->sock_command, msg_cwd_fail);
		goto EXIT;
	}
	int len_user_command = strlen(puser->command_block);
	if(puser->command_block[len_user_command - 1] != '/')
	{
		//对user的当前路径进行标准化
		strcat(puser->command_block, "/");
	}
	if(puser->command_block[0] == '/')
	{update_user_path(&puser->current_path, puser->command_block);}
	else
	{add_user_path(&puser->current_path, puser->command_block);}
	easy_reply(puser->sock_command, msg_cwd_ok);
EXIT:
	free(path_abs);
	free(command);
	return 0;
}

int process_list(struct user* puser, char* para)
{
	return process_block_operation(puser, para, BLOCK_UP_LIST);
}
int process_rmd(struct user* puser, char* para)
{
	if(!path_legacy_check(para))
	{
		easy_reply(puser->sock_command, msg_path_illegal);
		return 0;
	}
	strcpy(puser->command_block, para);
	char *path_abs = build_file_path(puser);
	int status = remove(path_abs);
	if(status == 0)
		easy_reply(puser->sock_command, msg_rmd_ok);
	else
		easy_reply(puser->sock_command, msg_rmd_failed);
	free(path_abs);
	return 0;
}
int process_default(struct user* puser, char* para)
{
	easy_reply(puser->sock_command, msg_invalid_syntax);
	return 0;
}
