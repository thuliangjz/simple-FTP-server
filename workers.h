#ifndef WORKERS
#define WORKERS
#include"user.h"
extern int push_request(struct user* puser);		//注意一个用户在任意时刻只能有一个未完成的传输任务
extern int worker_init(int count_worker);		//留在main中进行调用,若成功则返回0, 否则返回1
#endif
