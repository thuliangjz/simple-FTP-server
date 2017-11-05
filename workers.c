#include"workers.h" 
#include"utils.h"
#include<sys/socket.h>
#include<pthread.h>
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/file.h>
#include<fcntl.h>

#define LENGTH_QUEUE COUNT_USER + 1
struct queue_block_requests
{
	int head;	
	int tail;	//tail 永远指向空的位置，即如果head == tail,则整个队列为空
	struct user *pusers[LENGTH_QUEUE];
};

struct thread_data
{
	char data[8];
};


struct queue_block_requests que_reqs;
pthread_mutex_t lock_queue;	//访问请求队列时的锁
pthread_mutex_t lock_system;	//执行系统调用的锁
pthread_cond_t cond_que_empty;
pthread_mutex_t lock_sleep;

typedef int (*block_processor)(struct user* puser, struct thread_data* p_thread_data);
int block_process_retr(struct user* puser, struct thread_data* p_thread_data);
int block_process_stor(struct user* puser, struct thread_data* p_thread_data);
int block_process_list(struct user* puser, struct thread_data* p_thread_data);

block_processor block_proc_lst[] = {block_process_retr, block_process_stor, block_process_list};


int push_request(struct user* puser)
{
	int empty = FALSE;
	pthread_mutex_lock(&lock_queue);
	puser->is_transmitting = TRUE;
	que_reqs.pusers[que_reqs.tail] = puser;
	if(que_reqs.tail == que_reqs.head)
		empty = TRUE;
	que_reqs.tail += 1;
	que_reqs.tail = que_reqs.tail == LENGTH_QUEUE ? 0 : que_reqs.tail;
	if(que_reqs.tail == que_reqs.head)
	{
		pthread_mutex_unlock(&lock_queue);
		return -1;
	}
	pthread_mutex_unlock(&lock_queue);
	if(empty)
		pthread_cond_signal(&cond_que_empty);
	return 0;
}

struct user* get_request()
{
	struct user* puser = NULL;
	//注意，之所以采用循环的方式完成，是由于在Linux的signal实现中，一次signal可能唤醒两个正在等待的线程
	while(! puser)
	{
		pthread_mutex_lock(&lock_queue);
		if(que_reqs.head == que_reqs.tail)
		{
			//队列为空
			pthread_mutex_unlock(&lock_queue);
			//获取锁并睡眠
			pthread_mutex_lock(&lock_sleep);
			pthread_cond_wait(&cond_que_empty, &lock_sleep);
			pthread_mutex_unlock(&lock_sleep);
			continue;
		}
		//到这里时必定已经获取了队列的锁且head != tail
		puser = que_reqs.pusers[que_reqs.head];
		que_reqs.pusers[que_reqs.head] = NULL;
		++que_reqs.head;
		que_reqs.head = que_reqs.head == LENGTH_QUEUE ? 0 : que_reqs.head;
		pthread_mutex_unlock(&lock_queue);
	}
	return puser;
}

void *worker_loop(void *arg)
{
	arg = (struct thread_data*)arg;
	struct user* puser;
	while(TRUE)
	{
		puser = get_request();
		block_proc_lst[puser->block_operation_type](puser, arg);
		close_connection(puser);
		puser->is_transmitting = FALSE;
	}
}

int worker_init(int count_worker)
{
	pthread_mutex_init(&lock_queue, NULL);
	pthread_mutex_init(&lock_system, NULL);
	pthread_mutex_init(&lock_sleep, NULL);
	pthread_cond_init(&cond_que_empty, NULL);
	que_reqs.head = que_reqs.tail = 0;
	int i;
	for(i = 0; i < LENGTH_QUEUE; ++i)
	{
		que_reqs.pusers[i] = NULL;
	}
	pthread_t thread_tmp;
	struct thread_data *names = (struct thread_data*)malloc(sizeof(struct thread_data) * count_worker);
	for(i = 0; i < count_worker; ++i)
	{
		sprintf(names[i].data ,"%d", i);	
		pthread_create(&thread_tmp, NULL, worker_loop, &names[i]);
	}
	return 0;
}

int check_data_connection(struct user* puser)
{
	int transfer_sock;
	if(puser->connect_status ==CONNECT_PORT)
	{
		transfer_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		//设置transfer_sock的阻塞时间
		struct timeval timeout;
		timeout.tv_sec = READ_TIME_OUT_DATA;
		timeout.tv_usec = 0;
		setsockopt(transfer_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
		
		if(connect(transfer_sock, (struct sockaddr*)&puser->port_cmd_addr, sizeof(puser->port_cmd_addr)) < 0)
		{
			easy_reply(puser->sock_command, msg_no_data_connection);
			close(transfer_sock);
			return -1;
		}
		
	}
	else
	{
		//connected using PASV
		if((transfer_sock = accept(puser->sock_data, NULL, NULL)) == -1)
		{
			easy_reply(puser->sock_command, msg_no_data_connection);
			return -1;
		}
	}
	return transfer_sock;
}

int block_process_retr(struct user* puser, struct thread_data* p_thread_data)
{
	if(!path_legacy_check(puser->command_block))
	{
		easy_reply(puser->sock_command, msg_path_illegal);
		return 0;
	}
	char *path_abs = build_file_path(puser);
	struct stat st;		//检测文件是否存在以及是否为普通文件(不是目录)
	int transfer_sock = -1, fd = -1, count_read = 0;
	char prompt[2 * SIZE_COMMAND];
	char buffer_file[4096];
	if(stat(path_abs, &st) == -1)
	{
		easy_reply(puser->sock_command, msg_open_file_failed);
		goto EXIT;
	}
	if(!S_ISREG(st.st_mode))
	{
		easy_reply(puser->sock_command, msg_open_file_failed);
		goto EXIT;
	}
	if((transfer_sock =  check_data_connection(puser)) == -1)
		goto EXIT;
	//trying to opening connection
	sprintf(prompt, "150 Opening BINARY mode data connection for %s (%lld bytes)\r\n", puser->command_block, (long long)st.st_size);
	easy_reply(puser->sock_command, prompt);

	//transferring file
	fd = open(path_abs, O_RDONLY);
	flock(fd, LOCK_SH);
	while((count_read = read(fd, buffer_file, 4096)) > 0)
	{
		if(!safe_write(transfer_sock, buffer_file, count_read))
		{
			easy_reply(puser->sock_command, msg_connection_crash);
			goto EXIT;
		}
	}
	//file transfer finished
	easy_reply(puser->sock_command, msg_file_transer_ok);
	//这里的EXIT只是相当于C++中的析构函数
EXIT:
	if(fd != -1)
	{flock(fd, LOCK_UN);close(fd);}
	if(transfer_sock != -1)
		close(transfer_sock);
	free(path_abs);
	return 0;
}

int block_process_stor(struct user* puser, struct thread_data* p_thread_data)
{
	if(!path_legacy_check(puser->command_block))
	{
		easy_reply(puser->sock_command, msg_path_illegal);
		return 0;
	}
	char *path_abs = build_file_path(puser);
	int transfer_sock = -1, fd = -1;
	if((transfer_sock = check_data_connection(puser)) == -1)
		goto EXIT;
	fd = open(path_abs, O_WRONLY | O_CREAT | O_TRUNC,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if(fd == -1)
	{
		easy_reply(puser->sock_command, msg_file_create_failed);
		goto EXIT;
	}
	easy_reply(puser->sock_command, msg_stor_prepared);
	flock(fd, LOCK_EX);	//采用排它锁，在写的时候任何人不能读文件	
	char buffer[4096];
	int count_read = 0;
	while((count_read = read(transfer_sock, buffer, 4096)) > 0)
	{
		write(fd, buffer, count_read);
	}
	if(count_read < 0)
	{
		easy_reply(puser->sock_command, msg_connection_crash);
		goto EXIT;
	}
	easy_reply(puser->sock_command, msg_file_transer_ok);
EXIT:
	if(transfer_sock != -1)
		close(transfer_sock);
	if(fd != -1)
	{flock(fd, LOCK_UN);close(fd);}
	free(path_abs);
	return 0;
}

int block_process_list(struct user* puser, struct thread_data* p_thread_data)
{
	strcpy(puser->command_block, "./");
	char *path_abs = build_file_path(puser);
	char *command_create = (char*)malloc(strlen(path_abs) * sizeof(char) + 128);
	char command_rm[100], name_file[20];
	char *format_create = "ls -l -a %s | awk -F \" \" '{if(NR>3){print $1\"\\t\"$2\"\\t\"$5\"\\t\"$6$7\"\\t\"$8\"\\t\"$9}}' > %s", *format_rm = "rm -r %s", *format_file = "thread_tmp%s.txt";
	sprintf(name_file, format_file, p_thread_data->data);
	sprintf(command_create, format_create, path_abs, name_file);
	sprintf(command_rm, format_rm, name_file);
	system(command_create);
	//以下部分仿照retr传输文件
	int transfer_sock = -1, fd = -1;
	if((transfer_sock = check_data_connection(puser)) == -1)
		goto EXIT;
	
	//trying to opening connection
	easy_reply(puser->sock_command, msg_list_prepare);

	//transferring file
	fd = open(name_file, O_RDONLY);
	flock(fd, LOCK_SH);
	int count_read = 0;
	char buffer_file[4096];
	while((count_read = read(fd, buffer_file, 4096)) > 0)
	{
		if(!safe_write(transfer_sock, buffer_file, count_read))
		{
			easy_reply(puser->sock_command, msg_connection_crash);
			goto EXIT;
		}
	}
	//file transfer finished
	easy_reply(puser->sock_command, msg_file_transer_ok);

EXIT:	
	if(transfer_sock != -1) 
		close(transfer_sock);
	if(fd != -1)
	{flock(fd, LOCK_UN); close(fd);}
	free(path_abs);
	free(command_create);
	system(command_rm);
	return 0;
}

