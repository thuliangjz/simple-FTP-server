#ifndef PROCESSORS
#define PROCESSORS
#include"user.h"
extern int process_user(struct user* puser, char* para);
extern int process_pass(struct user* puser, char* para);
extern int process_retr(struct user* puser, char* para);
extern int process_stor(struct user* puser, char* para);
extern int process_quit(struct user* puser, char* para);
extern int process_syst(struct user* puser, char* para);
extern int process_type(struct user* puser, char* para);
extern int process_port(struct user* puser, char* para);
extern int process_pasv(struct user* puser, char* para);
extern int process_mkd(struct user* puser, char* para);
extern int process_cwd(struct user* puser, char* para);
extern int process_list(struct user* puser, char* para);
extern int process_rmd(struct user* puser, char* para);
extern int process_default(struct user* puser, char* para);
#endif
