#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<unistd.h>
#include<pthread.h>
#include<mysql/mysql.h>
#include<sys/time.h>
#include<sys/epoll.h>
/*
char sql_insert[200];
sprintf(sql_insert, "INSERT INTO table values('%s','%d');", name, age);
*/

/*服务端*/
#define PORT 8848
#define send_length 200

MYSQL *con;         //mysql连接
MYSQL_RES *res = NULL;  //mysql记录集
MYSQL_ROW row;      //字符串数组，mysql记录行
/*mysql*/
char *user = "debian-sys-maint";                                            
char *key = "NHqLsqHfc2mBjaKt";
char *host = "localhost";
char *db = "user";

typedef struct ONLINE
{
	char name[20];
	int socket;
	struct ONLINE *next;
}online;

online *head = NULL;

void create(online **head)
{
	online *p;
	p = (online *)malloc(sizeof(online));
	*head = p;
	p->next = NULL;
}

online *add_online(online *head, char *name, int socket)
{
	online *add = (online *)malloc(sizeof(online));
	strcpy(add->name, name);
	add->socket = socket;
	online *temp = head;
	while(temp->next != NULL)
		temp = temp->next;
	temp->next = add;
	return head;
}

online *del_online(online *head, int socket)
{
	online *rev = head->next;	
	online *pre = head;
	while(rev != NULL)
	{
		if(rev->socket == socket)
		{
			pre->next = rev->next;
			return head;
		}
		else
		{
			pre = rev;
			rev = rev->next;
		}
	}
	return head;
}

int find_online(online *head, char *str)
{
	online *temp = head;
	while(temp != NULL)
	{
		if(strcmp(temp->name, str) == 0)
			return temp->socket;
		else
			temp = temp->next;
	}
	return 0;
}


pthread_mutex_t mutex;

/*线程池任务*/
void pool_add_worker(void(*process)(char *str, int socket), char *str, int socket);
void *thread_routine(void *arg);
/*初始化线程池*/
void pool_init(int max_thread_num);
/*销毁线程池*/
int pool_destroy();
/*线程池回调*/
void process(char *str, int socket);
/*错误处理*/
void my_err(char *str, int line);
/*用户注册*/
void user_login(char *str, int socket); 
/*用户登录*/
void user_enter(char *str, int socket);
/*找回密码*/
void find_passwd(char *str, int socket);
/*删除离线用户套接字*/
void rm_socket(int socket);
/*发送消息*/
void send_message(char *str, int socket);
/*添加好友*/
void add_friend(char *str, int socket);
/*是否添加*/
void agree_friend(char *str, int socket);
/*删除好友*/
void rm_friend(char *str, int socket);
/*查看好友列表*/
void watch_friend(char *str, int socket);
/*查看历史记录*/
void find_history(char *str, int socket);
/*屏蔽好友消息*/
void shield_friend(char *str, int socket);
/*创建群聊*/
void create_group(char *str, int socket);
/*删除群聊*/
void rm_group(char *str, int socket);
/*申请加群*/
void add_group(char *str, int socket);
/*是否同意*/
void aod_group(char *str, int socket);
/*发送群消息*/
void send_group(char *str, int socket);
/*查看群聊天记录*/
void group_his(char *str, int socket);
/*查看加入的群名*/
void watch_group(char *str, int socket);
/*查看群成员*/
void watch_member(char *str, int socket);
/*设置管理员*/
void set_ad(char *str, int socket);
/*踢人*/
void del_member(char *str, int socket);
/*接收文件*/
void recv_filename(char *str, int socket);
/*是否同意接收*/
void agg_file(char *str, int socket);
/*发送文件*/
void send_file(char *str, int socket);


/*任务结构*/
typedef struct worker
{
	void (*process)(char *str, int socket);
	char str[200];
	int socket;
	struct worker *next;
}create_worker;
/*线程池结构*/
typedef struct
{
	pthread_mutex_t queue_mutex;
	pthread_cond_t queue_cond;
	/*链表结构,线程池中有所等待任务*/
	create_worker *queue_head;
	/*是否销毁线程池*/
	int shutdown;
	pthread_t *threadid;
	/*线程池中允许的活动线程数目*/
	int max_thread_num;
	/*当前等待队列的任务数目*/
	int cur_queue_size;
}create_pool;


static create_pool *pool = NULL;
/*创建线程池*/
void pool_init(int max_thread_num)
{
	pool = (create_pool *)malloc(sizeof(create_pool));
	pthread_mutex_init(&(pool->queue_mutex), NULL);
	pthread_cond_init(&(pool->queue_cond), NULL);
	pool->queue_head = NULL;
	pool->max_thread_num = max_thread_num;
	pool->cur_queue_size = 0;
	pool->shutdown = 0;
	pool->threadid = (pthread_t *)malloc(sizeof(pthread_t) * max_thread_num);
	for(int i = 0; i < max_thread_num; i++)
	{
		pthread_create(&pool->threadid[i], NULL, thread_routine, NULL);
	}
}

/*错误处理函数*/
void my_err(char *str, int line)
{
	fprintf(stderr, "line: %d\n", line);
	perror(str);
	exit(-1);
}

/*向线程池中加入任务*/
void pool_add_worker(void (*process) (char *str, int socket), char *str, int socket)
{
	/*构建一个新任务*/
	create_worker *newworker = (create_worker*)malloc(sizeof(create_worker));
	newworker->process = process;
	memcpy(newworker->str, str, 200);
	newworker->socket = socket;
	newworker->next = NULL;
	pthread_mutex_lock(&pool->queue_mutex);
	/*将任务加入到等待队列*/
	create_worker *member = pool->queue_head;
	if(member != NULL)
	{
		while(member->next != NULL)
			member = member->next;
		member->next = newworker;
	}
	else
		pool->queue_head = newworker;
	assert(pool->queue_head != NULL);
	pool->cur_queue_size++;
	pthread_mutex_unlock(&pool->queue_mutex);
	/*等待队列有任务，唤醒一个等待的线程*/
	pthread_cond_signal(&pool->queue_cond);
}
/*销毁线程池， 等待队列中的任务不会在执行，但是正在运行的线程一定会把任务运行完后在退出*/
int pool_destroy()
{
	if(pool->shutdown)
		return -1;		//防止两次调用	
	pool->shutdown = 1;
	/*唤醒所有的等待线程，线程池要销毁了*/
	pthread_cond_broadcast(&pool->queue_cond);

	/*阻塞等待线程退出，否则就成僵尸了*/
	for(int i = 0; i < pool->max_thread_num; i++)
		pthread_join(pool->threadid[i], NULL);      //等待所有线程执行完毕再销毁
	free(pool->threadid);
	/*销毁等待队列*/
	create_worker *head = NULL;
	while(pool->queue_head != NULL)
	{
		head = pool->queue_head;
		pool->queue_head = pool->queue_head->next;
		free(head);
	}

	/*销毁条件变量和互斥量*/
	pthread_mutex_destroy(&pool->queue_mutex);
	pthread_cond_destroy(&pool->queue_cond);
	free(pool);
	pool = NULL;
}

void *thread_routine(void *arg)
{
	//printf("线程开始运行 %ld\n", pthread_self());
	while(1)
	{
		pthread_mutex_lock(&pool->queue_mutex);
		/*如果等待队列为0并且不销毁线程池，则处于阻塞状态*/
		while(pool->cur_queue_size == 0 && !pool->shutdown)
		{
			printf("thread: %ld is wait\n", pthread_self());
			pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
		}
	//	printf("shutdown is: %d\n", pool->shutdown);
		/*线程池要销毁了*/
		if(pool->shutdown)
		{
			/*遇到break, continue, return 等跳转语句，要记得先解锁*/
			pthread_mutex_unlock(&pool->queue_mutex);
			printf("thread %ld will exit\n", pthread_self());
			pthread_exit(NULL);
		}
		printf("thread %ld is starting to work\n", pthread_self());
		
		assert(pool->cur_queue_size != 0);
		assert(pool->queue_head != NULL);
		/*等待队列长度减１，并去除链表中的头元素*/
		pool->cur_queue_size--;
		create_worker *work = pool->queue_head;
		pool->queue_head = work->next;
		pthread_mutex_unlock(&pool->queue_mutex);

		/*调用回调函数，执行任务*/
		(work->process) (work->str, work->socket);
		free(work);
		work = NULL;
	}
	//这一句应该不可以到达执行
	pthread_exit(NULL);
}

/*回调函数*/
void process(char *str, int socket)
{
	printf("str = %s\n", str);
	//printf("socket =  %d\n", socket);
	if(strncmp(str, "login:", 6) == 0)             //注册用户
		user_login(str, socket);

	else if(strncmp(str, "enter:", 6) == 0)        //登录
		user_enter(str, socket);

	else if(strncmp(str, "find_passwd:", 12) == 0) //找回密码
		find_passwd(str, socket);

	else if(strncmp(str, "~add:", 5) == 0)         //添加好友
		add_friend(str, socket);

	else if(strncmp(str, "~rm:", 4) == 0)          //删除好友
 		rm_friend(str, socket);

	else if(strcmp(str, "~") == 0)      		   //查看好友列表
	 	watch_friend(str, socket);

	else if(strncmp(str, "~history:", 9) == 0)     //查看聊天记录
		find_history(str, socket);

	else if(strncmp(str, "~shield:", 8) == 0)      //屏蔽好友  
		 shield_friend(str,socket);

	else if(strncmp(str, "!create:", 8) == 0)      //创建群聊
		create_group(str, socket);

	else if(strncmp(str, "~rm_group:", 10) == 0)   //删除群聊
		rm_group(str, socket);

	else if(strncmp(str, "~add_g:", 7) == 0)       //申请加群
		add_group(str, socket);

	else if(strncmp(str, "~@@", 3) == 0)           //是否同意添加群聊
		aod_group(str, socket);	
	
	else if(strncmp(str, "~@", 2) == 0)      //是否同意添加好友
		agree_friend(str, socket);

	else if(strncmp(str, "~g:", 3) == 0)           //发送群消息
		send_group(str, socket);

	else if(strncmp(str, "~g_his:", 5) == 0)	   //查看历史群消息
		group_his(str, socket);

	else if(strncmp(str, "~w:", 3) == 0)           //查看群成员
		watch_member(str, socket);

	else if(strncmp(str, "w", 1) == 0)            //查看所加入的群
		watch_group(str, socket);

	else if(strncmp(str, "~set:", 5) == 0)         //设置管理员
		set_ad(str, socket);

	else if(strncmp(str, "~del:", 5) == 0)         //删除群成员
		del_member(str, socket);
	
	else if(strncmp(str, "~name:", 6) == 0) 	   //文件发送
		recv_filename(str, socket);
	
	else if(strncmp(str, "~$", 2) == 0)            //是否同意接收
		agg_file(str, socket);

	else if(strncmp(str, "&:", 2) == 0)            //发送文件
		send_file(str, socket);

	else if(strncmp(str, "~", 1) == 0)		       //发送消息
		send_message(str, socket);

}


/*解析用户注册的名字密码及密保问题*/
void user_login(char *str, int socket)
{
	char name[20] = {0};
	char passwd[20] = {0};
	char question[20] = {0};
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 3)
		{
			count++;
			i++;
			j = 0;
		}
		if(count == 1)               //将用户名解析出来
		{
			name[j++] = str[i];
			continue;
		}
		if(count == 2)
		{
			passwd[j++] = str[i];	//解析出密码
			continue;
		}
		if(count == 3)
			question[j++] = str[i];	//解析出回答
	}
	char sql_insert[100];
	sprintf(sql_insert, "INSERT INTO person(username, passwd, question) values('%s', '%s', '%s')",name, passwd, question);
	if(mysql_real_query(con, sql_insert, strlen(sql_insert)))
		my_err("INSERT", __LINE__);
	char flag[200] = "success";
	if(send(socket, flag, send_length, 0) < 0)
		my_err("send", __LINE__);
}

/*处理用户登录*/
void user_enter(char *str, int socket)
{
	char name[20] = {0};
	char passwd[20] = {0};
	char flag[200] = {0};
	int i, j = 0, count = 0;
	/*解析用户姓名及密码*/
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
		{
			count++;
			i++;
			j = 0;
		}
		if(count == 1)
			name[j++] = str[i];
		else if(count == 2)
			passwd[j++] = str[i];
	}
	/*将用户信息存储起来*/
	char *sql_select = "SELECT username, passwd from person";
	if(mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	/*查看是否有离线消息*/
	MYSQL_RES *r = NULL;
	MYSQL_ROW ro;
	char *sql_se = "SELECT name1, name2, message from record";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	r = mysql_store_result(con);
	/*上线通知好友*/
	char *sql_friend = "SELECT name1, name2 from friend";
	MYSQL_RES *inf = NULL;
	MYSQL_ROW rec;
	if( mysql_real_query(con, sql_friend, strlen(sql_friend)) )
		my_err("online", __LINE__);
	inf = mysql_store_result(con);
	/*判断好友是否在线*/
	char *sql_online = "SELECT username, socket from online";
	MYSQL_RES *tr = NULL;
	MYSQL_ROW hh;

	while(row = mysql_fetch_row(res))
	{
		/*登录成功*/
		if(strcmp(row[0], name) == 0 && strcmp(row[1], passwd) == 0)
		{
		
		/*判断用户是否已经登录*/
		
			if( mysql_real_query(con, sql_online, strlen(sql_online)) )
				my_err("online", __LINE__);
			tr = mysql_store_result(con);
			while(hh = mysql_fetch_row(tr))
			{
				if(strcmp(hh[0], name) == 0)
				{
					strcpy(flag, "fault");
					if(send(socket, flag, send_length, 0) < 0)
						my_err("send", __LINE__);
					return;
				}
			}
			/*登录成功后将套接字存入实时在线用户库中*/

			head = add_online(head, row[0], socket);         
			
			char sql_socket[150] = {0};
			sprintf(sql_socket, "INSERT INTO online(username, socket) values('%s', '%d')", row[0], socket);
			if( mysql_real_query(con, sql_socket, strlen(sql_socket)) )
				my_err("INSERT", __LINE__);
			strcpy(flag, "success");
			if(send(socket, flag, send_length, 0) < 0)
				my_err("send", __LINE__);
			/*上线通知好友*/
			while(rec = mysql_fetch_row(inf))
			{
				if(strcmp(name, rec[0]) == 0)
				{
					if( mysql_real_query(con, sql_online, strlen(sql_online)) )
						my_err("online", __LINE__);
					tr = mysql_store_result(con);
					while(hh = mysql_fetch_row(tr))
					{
						if(strcmp(hh[0], rec[1]) == 0)
						{
							char message[200] = {0};
							sprintf(message, "#您的好友%s上线啦", rec[0]);
							printf("message = %s\n", message);
							if( send(atoi(hh[1]), message, send_length, 0) < 0)
								my_err("send", __LINE__);
						}
					}
				}
				else if(strcmp(name, rec[1]) == 0)
				{
					if( mysql_real_query(con, sql_online, strlen(sql_online)) )
						my_err("online", __LINE__);
					tr = mysql_store_result(con);
					while(hh = mysql_fetch_row(tr))
					{
						if(strcmp(hh[0], rec[0]) == 0)
						{
							char message[200] = {0};
							sprintf(message, "#您的好友%s上线啦", rec[1]);
							printf("message = %s\n", message);
							if( send(atoi(hh[1]), message, send_length, 0) < 0)
								my_err("send", __LINE__);
						}
					}
				}
			}

			/*用户上线，查询是否有离线消息 如果有发送离线消息*/
			while(ro = mysql_fetch_row(r))
			{
				if(strcmp(name, ro[1]) == 0)
				{
					char *off = "off:";
					char off_message[200] = {0};
					strcpy(off_message, off);
					char *s = " send to you  ";
					strcat(off_message, ro[0]);
					strcat(off_message, s);
					strcat(off_message, ro[2]);
					printf("off_message = %s\n", off_message);
					if(send(socket, off_message, send_length, 0) < 0)
						my_err("send", __LINE__);
					/*发送成功后，将消息从数据库中删除*/
					char sql_delete[100];
					sprintf(sql_delete, "DELETE from record where name1='%s'&&name2='%s'&&message='%s'", ro[0],ro[1],ro[2]);
					mysql_real_query(con, sql_delete, strlen(sql_delete));
				}
			}
			return;
		}
	}
	strcpy(flag, "fault");
	if(send(socket, flag, send_length, 0) < 0)
		my_err("send", __LINE__);
}

/*找回用户名*/
void find_passwd(char *str, int socket)
{
	char name[20] = {0};
	char question[20] = {0};
	int i, j = 0, count = 0;
	/*解析出问题和名字*/
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
		{
			count++;
			i++;
			j = 0;
		}
		if(count == 1)
			name[j++] = str[i];
		else if(count == 2)
			question[j++] = str[i];
	}
	printf("name: %s  question: %s\n", name, question);
	char *sql_select = "SELECT username, passwd, question from person";
	if(mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(row[0], name) == 0 && strcmp(row[2], question) == 0)
		{
			char p[200] = {0};
			strcpy(p, row[1]);
			if(send(socket, p, 200, 0) < 0)
				my_err("send", __LINE__);
			return;
		}
	}
	if(send(socket,"fault", 6, 0) < 0)
		my_err("send", __LINE__);
	return;	
}

/*将离线用户的套接字删掉*/
void rm_socket(int socket)
{
	char sql_delete[150] = {0};
	sprintf(sql_delete, "DELETE FROM online where socket=%d", socket);
	if( mysql_real_query(con, sql_delete, strlen(sql_delete)) )
		my_err("sql_delete", __LINE__);
}

/*客户端之间发送消息*/
void send_message(char *str, int socket)
{
	char name[20] = {0};
	char name2[20] = {0};
	char send_message[200] = {0};
	char message[200] = {0};
	int i, j = 0, count = 0;
	int sock_fd;
	int flag = 0;                  //判断用户是离线还是被标记
	for(i = 1; ; i++)
	{
		if(str[i] == ':' && count < 1)
		{
			count++;
			i++;
		}
		if(count == 1)
		{
			strcpy(message, &str[i]);
			break;
		}
		else
			name[j++] = str[i];
	}
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	printf("send name = %s send message = %s\n", name, message);
	/*检查好友是否被屏蔽*/
	pthread_mutex_lock(&mutex);
	char *sql_shield = "SELECT name1, name2, status from friend";
	MYSQL_RES *re = NULL;
	MYSQL_ROW ro;
	if( mysql_real_query(con, sql_shield, strlen(sql_shield)) )
		my_err("shield", __LINE__);
	re = mysql_store_result(con);

	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(socket == atoi(row[1]))
		{
			strcpy(name2, row[0]);
			/*判断是否被屏蔽*/
			while(ro = mysql_fetch_row(re))
			{
				if( (strcmp(name, ro[0]) == 0 && strcmp(name2, ro[1]) == 0) || (strcmp(name2, ro[0]) == 0 && strcmp(name, ro[1]) == 0) )
				{
					if(atoi(ro[2]) == 0)
						return;
				}
			}
			strcpy(send_message, row[0]);
			send_message[strlen(send_message)] = ':';
			send_message[strlen(send_message)] = '\0';
			count++;
		}
		if(count == 2)
		{
			strcat(send_message, message);
			if(send(sock_fd, send_message, 200, 0) < 0)
				my_err("send", __LINE__);
			/*将消息添加到聊天记录中*/
			char sql_history[100] = {0};
			sprintf(sql_history, "INSERT INTO history(name1, name2, message)values('%s','%s','%s')", name2, name, message);
			printf("sql_history = %s\n", sql_history);
		//	pthread_mutex_lock(&mutex);
				if	( mysql_real_query(con, sql_history, strlen(sql_history)) )
					printf("======================================================================================\n");
					//my_err("history", __LINE__);
			pthread_mutex_unlock(&mutex);
			return;
		}	
	}

	/*保存离线用户消息*/
	send_message[strlen(send_message) - 1] = '\0';
	printf("send_message = %s, name = %s, message = %s\n", send_message, name, message);
	char sql_message[200] = {0};
	sprintf(sql_message, "INSERT INTO record(name1, name2, message)values('%s', '%s', '%s')", send_message, name, message);
	mysql_real_query(con, sql_message, strlen(message));
	if( mysql_real_query(con, sql_message, strlen(sql_message)) )
		my_err("insert", __LINE__);
}

/*添加好友*/
void add_friend(char *str, int socket)
{
	int sock_fd;
	char name[20] = {0};
	char message[200] = "add:";
	int i, j= 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
		{
			i++;
			count++;
		}
		if(count == 1)
			name[j++] = str[i];
	}
	printf("name = %s\n", name);
	char *sql_add = "SELECT username, socket FROM online";
	if( mysql_real_query(con, sql_add, strlen(sql_add)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(socket == atoi(row[1]))
		{
			strcat(message, row[0]);
			char *temp = ":want to add you";
			strcat(message, temp);
			count++;
		}
		if(count == 2)
		{
			printf("add message = %s\n", message);
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
	}
}

/*同意添加好友*/
void agree_friend(char *str, int socket)
{
	char myname[200] = "#";
	char name[20] = {0};
	int i, j = 0, count = 0;
	int sock_fd;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
		{
			count++;
			i++;
		}
		if(count == 1)
			name[j++] = str[i];
	}
	char *sql_add = "SELECT username, socket FROM online";
	if( mysql_real_query(con, sql_add, strlen(sql_add)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(atoi(row[1]) == socket)
		{
			strcat(myname, row[0]);
			count++;
		}
		if(count == 2)
			break;
	}
	if(strncmp(str, "~@disagree", 10) == 0)
	{
		char *p = " disagree add you";
		strcat(myname, p);
		if(send(sock_fd, myname, send_length, 0) < 0)
			my_err("send", __LINE__);
		return;
	}
	else
	{
		char sql[100] = {0};
		sprintf(sql, "INSERT INTO friend(name1, name2, status) values('%s', '%s', '1')", name, &myname[1]);
		printf("sql = %s\n", sql);
		if( mysql_real_query(con, sql, strlen(sql)) )
			my_err("add", __LINE__);
		char *p = " agree your application";
		strcat(myname, p);
		if(send(sock_fd, myname, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
}

/*删除好友*/
void rm_friend(char *str, int socket)
{
	char name1[20] = {0};                    //用户名1
	char name2[20] = {0};                    //用户名2
	int sock_fd = 0;
	int i, count = 0, j = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
		{
			count++;
			i++;
		}
		if(count == 1)
			name1[j++] = str[i]++; 
	}
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name1, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(atoi(row[1]) == socket)
		{
			strcpy(name2, row[0]);
			count++;
		}
		if(count == 2)
			break;
	}
	char sql_del[100] = {0};
	sprintf(sql_del, "DELETE from friend where name1='%s'&&name2='%s' || name1='%s'&&name2='%s'", name1, name2, name2, name1);
	if(mysql_real_query(con, sql_del, strlen(sql_del)))
		my_err("delete", __LINE__);
	char message[200] = "#";
	char *p = " delete you --";
	strcat(message, name2);
	strcat(message, p);
	if(sock_fd > 0)                    //如果用户在线通知，不在线则不通知
	{
		if(send(sock_fd, message, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
}

/*查看好友列表*/
void watch_friend(char *str, int socket)
{
	char name[20] = {0};
	char message[200] = {0};
	int i, j = 0, count = 0;
	int sock_fd = 0;
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name, row[0]);
			break;
		}
	}
	char *sql_se = "SELECT name1, name2 from friend";
	MYSQL *q = NULL;
	MYSQL_RES *re = NULL; 
	MYSQL_ROW ro;
	q = mysql_init(q);
	mysql_real_connect(q, host, user, key, db, 0, NULL, 0);
	if( mysql_real_query(q, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	re = mysql_store_result(q);
	int flag = 0;
	char *temp = "@friend:";
	char *status1 = " online";
	char *status2 = " offline";
	while(ro = mysql_fetch_row(re))
	{
		if(strcmp(name, ro[0]) == 0 || strcmp(name, ro[1]) == 0)       //找到自己
		{
			printf("ro[0] =  %s  ro[1] = %s\n", ro[0], ro[1]);
			strcat(message, temp);
		if( mysql_real_query(con, sql_select, strlen(sql_select)) )
			my_err("select", __LINE__);
		res = mysql_store_result(con);
			while(row = mysql_fetch_row(res))
			{
				if( (strcmp(ro[1], row[0]) == 0 && strcmp(name, row[0])) )
				{
					flag = 1;
					strcat(message, row[0]);
					strcat(message, status1);
					printf("message = %s\n", message);
					if(send(socket, message, send_length, 0) < 0)
						my_err("send", __LINE__);
				}
				else if(strcmp(ro[0], row[0]) == 0 && strcmp(name, row[0]))
				{
					flag = 1;
					strcat(message, row[0]);
					strcat(message, status1);
					printf("message = %s\n", message);
					if(send(socket, message, send_length, 0) < 0)
						my_err("send", __LINE__);
				}
			}
			if(flag == 0)
			{
				if(strcmp(name, ro[0]) == 0)
					strcat(message, ro[1]);
				else
					strcat(message, ro[0]);
				strcat(message, status2);
				printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
			}
			flag = 0;
			memset(message, '\0', sizeof(message));
		}
	}
}

/*查看聊天记录*/
void find_history(char *str, int socket)
{
	char name1[20] = {0};                 
	char name2[20] = {0};				   
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			name2[j++] = str[i];
	}
	printf("history name2 = %s\n", name2);
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name1, row[0]);
			break;
		}
	}
	char *sql_se = "SELECT name1, name2, message from history";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name1, row[0]) == 0 && strcmp(name2, row[1]) == 0)
		{
			char message[200] = "history:";
			strcat(message, row[0]);
			message[strlen(message)] = ':';
			strcat(message, row[2]);
			printf("histor message = %s\n", message);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
		else if(strcmp(name1, row[1]) == 0 && strcmp(name2, row[0]) == 0)
		{
			char message[200] = {0};
			strcat(message, row[1]);
			message[strlen(message)] = ':';
			char *p = "                    ";
			sprintf(message, "history: %s %s:%s", p, name2, row[2]);
			printf("histor message = %s\n", message);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
	}
}

/*屏蔽好友消息*/
void shield_friend(char *str, int socket)
{
	char name1[20] = {0};
	char name2[20] = {0};
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			name2[j++] = str[i];
	}
	/*找出发起屏蔽的人*/
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	/*找到好友列表中他们的位置*/
	char *sql_friend = "SELECT name1, name2, status from friend";
	MYSQL_RES *re = NULL;
	MYSQL_ROW ro;
	if( mysql_real_query(con, sql_friend, strlen(sql_friend)) )
		my_err("friend", __LINE__);
	re = mysql_store_result(con);

	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name1, row[0]);
			while(ro = mysql_fetch_row(re))
			{
				if( strcmp(name1, ro[0]) == 0 && strcmp(name2, ro[1]) == 0 )
				{
					char up[100] = {0};
					sprintf(up, "UPDATE friend set status='0' where name1='%s'&&name2='%s'", name1, name2);
					if(mysql_real_query(con, up, strlen(up)) )
						my_err("update", __LINE__);
				}
				else if(strcmp(name1, ro[1]) == 0 && strcmp(name2, ro[0]) == 0)
				{
					char up[100] = {0};
					sprintf(up, "UPDATE friend set status='0' where name1='%s'&&name2='%s'", name2, name1);
					if(mysql_real_query(con, up, strlen(up)) )
						my_err("update", __LINE__);
				}
			}
		}
	}
}

/*创建群聊*/
void create_group(char *str, int socket)
{
	char name[20] = {0};              //群主名
	char group_name[20] = {0};        //群名
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			group_name[j++] = str[i];
	}
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name, row[0]);              //找到群主名
			break;
		}
	}
	printf("group_name = %s  boss name = %s\n", group_name, name);
	char sql_create[100];
	sprintf(sql_create, "INSERT INTO groups(group_name, member, status)values('%s','%s','2')", group_name,name);
	printf("sql_create = %s\n", sql_create);
	if( mysql_real_query(con, sql_create, strlen(sql_create)) )
		my_err("create", __LINE__);
	char sql_insert[200] = {0};
	/*创建成功后给客户端发送成功消息*/
	char flag[200] = "success";
	if(send(socket, flag, send_length, 0) < 0)
		my_err("send", __LINE__);
}

/*群的解散*/
void rm_group(char *str, int socket)
{
	char group_name[20] ={0};
	char name[20] = {0};
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			group_name[j++] = str[i];
	}
	printf("rm group = %s\n", group_name);
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )	
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name, row[0]);
			break;
		}
	}
	/*查找该用户是否为群主*/
	int flag = 0; //标记群是否被删除
	char *sql_se ="SELECT group_name, member, status from groups";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		/*满足删除条件*/
		if(strcmp(row[0], group_name) == 0 && strcmp(name, row[1]) == 0 && atoi(row[2]) == 2)
		{
			char sql_de[100] = {0};
			flag = 1;
			sprintf(sql_de, "delete from groups where group_name='%s'", group_name);
			if( mysql_real_query(con, sql_de, strlen(sql_de)) )
				my_err("delete", __LINE__);
			break;
		}
	}
	if(flag == 0)
	{
		char message[200] = "#小伙子，大哥的群你也敢删?";
		if(send(socket, message, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
	else
	{
		char message[200] = "#删群成功\n";
		if(send(socket, message, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
}

/*申请加群*/
void add_group(char *str, int socket)
{
	char name1[20] = {0};	  //群主的名字
	char name2[20] = {0};     //申请加群的名字
	int sock_fd = 0;
	char group_name[20] = {0};
	int i, j = 0, count = 0;
	/*解析出群名*/
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] ==':' &&count < 1)
			count++, i++;
		if(count == 1)
			group_name[j++] = str[i];
	}
	printf("group_name = %s\n", group_name);
	/*找出申请加群的名字*/
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name2, row[0]);
			printf("群成员\n");
			break;
		}
	}
	printf("name2 = %s\n", name2);
	/*找出该群的群主*/
	char *sql_se = "SELECT group_name, member, status from groups";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(group_name, row[0]) == 0 && atoi(row[2]) == 2)
		{
			strcpy(name1, row[1]);
			break;
		}
	}
	printf("name1 = %s\n", name1);
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name1, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			char message[200] = "add_group:";
			char *p = " want to join your group ";
			strcat(message, name2);
			strcat(message, p);
			strcat(message, group_name);
			printf("message = %s\n", message);
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
			break;
		}
	}
}

/*是否同意入群*/
void aod_group(char *str, int socket)
{
	char name[20] = {0};          //申请入群用户
	char name1[20] = {0};		  //群主
	char group[20] = {0};
	int sock_fd = 0;
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
			i++, count++, j = 0;
		if(count == 1)
			group[j++] = str[i];
		if(count == 2)
			name[j++] = str[i];
	}
	printf("add group = %s, add name = %s\n", group, name);
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(socket == atoi(row[1]))
		{
			strcpy(name1, row[0]);
			count++;
		}	
		if(count == 2)
			break;
	}
	if(strncmp(str, "~@@a", 4) == 0)
	{
		char sql_add[100] = {0};
		sprintf(sql_add, "INSERT INTO groups(group_name, member, status)values('%s','%s','0')", group, name);
		printf("sql_add = %s\n", sql_add);
		if( mysql_real_query(con, sql_add, strlen(sql_add)) )
			my_err("sql_add", __LINE__);
		if(sock_fd != 0)
		{
			char message[200];
			sprintf(message, "# %s agree you to join %s", name1, group);
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
		else
			return;
	}
	else
	{
		if(sock_fd != 0)
		{
			char message[200] = "#你被残忍的拒绝入群了";
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
		else
			return;
	}
	
}

/*发送群消息*/
void send_group(char *str, int socket)
{
	char group_name[20] = {0};
	char send_name[20] = {0};
	char send_message[200] = {0};
	int i, j = 0, count = 0;
	for(i = 3; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
		{
			count++;
			i++;
			j = 0;
		}
		if(count == 0)
			group_name[j++] = str[i];
		if(count == 1)
		{
			strcpy(send_message, &str[i]);
			break;
		}

	}
	/*找出发消息的用户名*/
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(send_name, row[0]);
			printf("send_name = %s\n", send_name);
			break;
		}
	}
	printf("group name = %s  send_name = %s send_message = %s\n", group_name, send_name, send_message);
	/*查找该群中的成员*/
	char *sql_se = "SELECT group_name, member from groups";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);

	/*查找该成员的套接字*/
	MYSQL_RES *re = NULL;
	MYSQL_ROW ro;
	int sock_fd;           //保存查找到的用户套接字
	char message[200];
	sprintf(message, "群:%s:用户%s:发送一条消息:%s", group_name, send_name, send_message);
	printf("group message = %s\n", message);
	
	/*保存聊天记录*/
	char sql_his[200] = {0};
	sprintf(sql_his, "INSERT INTO group_his(group_name, member, message)values('%s','%s','%s')", group_name, send_name,send_message);
	printf("sql_his = %s\n", sql_his);
	int flag = 0;                //标志该用户是否在线

	/*保存离线消息*/
	char sql_rec[200] = {0};

	while(row = mysql_fetch_row(res))
	{
		if(strcmp(group_name, row[0]) == 0)             //找到该群
		{
			if(strcmp(send_name, row[1]))               //判断是否是发消息人自己
			{
				/*向群成员发送消息*/
				if( mysql_real_query(con, sql_select, strlen(sql_select)) )
					my_err("select", __LINE__);
				re = mysql_store_result(con);
				while(ro = mysql_fetch_row(re))
				{
					printf("ro[0] = %s", ro[0]);
					if(strcmp(ro[0], row[1]) == 0)
					{
						flag = 1;
						sock_fd = atoi(ro[1]);
						if(send(sock_fd, message, send_length, 0) < 0)
							my_err("send",__LINE__);
						break;
					}
					
				}
				if(flag == 0)
				{
					sprintf(sql_rec, "INSERT INTO record(name1, name2, message)values('%s','%s','%s')", group_name, row[1], send_message);
					if( mysql_real_query(con, sql_rec, strlen(sql_rec)) )
						my_err("record", __LINE__);
				}
			}
			flag = 0;
		}
	}
	if( mysql_real_query(con, sql_his, strlen(sql_his)) )
		my_err("history", __LINE__);
}

/*查看群聊天记录*/
void group_his(char *str, int socket)
{
	char group_name[20] = {0};
	char name[20] = {0};
	strcpy(group_name, &str[7]);
	printf("group_name = %s\n", group_name);
	/*找出查看记录的用户名*/
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name, row[0]);
			break;
		}
	}
	printf("name = %s\n", name);
	char *sql_se = "SELECT group_name, member, message from group_his";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(group_name, row[0]) == 0)
		{
			/*记录不是自己发出*/
			if( strcmp(name, row[1]) )
			{
				char message[200] = {0};
				char *p = "                                     ";
				sprintf(message, "g_his:%s%s : %s", p, row[1], row[2]);
				printf("g_his = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
			}
			else
			{
				char message[200] = {0};
				sprintf(message, "g_his:%s :%s", name, row[2]);
				printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
			}
		}
	}
}

/*查看加入的群名*/
void watch_group(char *str, int socket)
{
	char name[20] = {0};
	/*解析出用户名*/
	char *sql_se = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(name, row[0]);
			break;
		}
	}
	printf("name nnn = %s\n", name);
	/*找出该用户所加入的群聊*/
	char *sql_select = "SELECT group_name, member from groups";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		char message[200] = "g:";
		if(strcmp(name, row[1]) == 0)
		{
			strcat(message, row[0]);
			printf("message = %s\n", message);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
			memset(message, 0, sizeof(message));
		}
	}
}

/*查看群成员*/
void watch_member(char *str, int socket)
{
	char group_name[20] = {0};
	strcpy(group_name, &str[3]);
	/*找出该群所有的成员*/
	char *sql_select = "SELECT group_name, member, status from groups";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);

	/*判断该成员是否在线*/
	char *sql_se = "SELECT username, socket from online";
	MYSQL_RES *re = NULL;
	MYSQL_ROW ro;
	int flag = 0;           //标志是否在线
	char message[200] = {0};

	/*查看群成员*/
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(group_name, row[0]) == 0)
		{
			if( mysql_real_query(con, sql_se, strlen(sql_se)) )
				my_err("select", __LINE__);
			re = mysql_store_result(con);
			/*判断是否在线*/
			while(ro = mysql_fetch_row(re))
			{
				if(strcmp(row[1], ro[0]) == 0 && atoi(row[2]) == 2) //在线且是群主
				{
					flag = 1;
					sprintf(message, "~member:%s(群主):online", row[1]);
					printf("message = %s\n", message);
					if(send(socket, message, send_length, 0) < 0)
						my_err("send", __LINE__);
					break;
				}
				else if(strcmp(row[1], ro[0]) == 0 && atoi(row[2]) == 1) //在线是管理员
				{
					flag = 1;
					sprintf(message, "~member:%s(管理员):online", row[1]);
					printf("message = %s\n", message);
					if(send(socket, message, send_length, 0) < 0)
						my_err("send", __LINE__);
					break;
				}
				else if(strcmp(row[1], ro[0]) == 0 && atoi(row[2]) == 0) 
				{
					flag = 1;
					sprintf(message, "~member:%s:online", row[1]);
					printf("message = %s\n", message);
					if(send(socket, message, send_length, 0) < 0)
						my_err("send", __LINE__);
					break;
				}
			}
			if(flag == 0)
			{
				if(atoi(row[2]) == 2) //离线群主
					sprintf(message, "~member:%s(群主):offline", row[1]);
				else if(atoi(row[2]) == 1)
					sprintf(message, "~member:%s(管理员):offline", row[1]);
				else if(atoi(row[2]) == 0) 
					sprintf(message, "~member:%s:offline", row[1]);
				printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
			}
		}
		memset(message, 0, sizeof(message));
		flag = 0;
	}
}

/*设置管理员*/
void set_ad(char *str, int socket)
{
	char own_name[20] = {0};
	char name[20] = {0};      //被设置管理员的人名
	char group_name[20] = {0}; //群名
	int sock_fd = 0;
	int i, j = 0, count = 0;
	for(i = 5; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			i++, count++;
		if(count == 0)
			group_name[j++] = str[i];
		if(count == 1)
		{
			strcpy(name, &str[i]);
			break;
		}
	}
	printf("group_name = %s name = %s\n",group_name, name);
	char *sql_se = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		else if(socket == atoi(row[1]))
		{
			strcpy(own_name, row[0]);
			count++;
		}
		if(count == 2)
			break;
	}
	printf("own_name = %s\n", own_name);
	char *sql_select = "SELECT group_name, member, status from groups";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(group_name, row[0]) == 0 && strcmp(own_name, row[1]) == 0 && atoi(row[2]) == 2)
		{
			char sql_update[100] = {0};
			sprintf(sql_update, "UPDATE groups set status=1 where group_name='%s'&& member='%s'", group_name, name);
			if( mysql_real_query(con, sql_update, strlen(sql_update)) )
				my_err("update", __LINE__);
			char message[200] = {0};
			sprintf(message, "#%s群的群主%s将你设置为管理员\n", group_name, own_name);
			if(sock_fd > 0)
			{
				if(send(sock_fd, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				return;
			}
			else
				return;
		}
	}
	char message[200] = "#小伙子，等升官了再来设置管理员权限把";
	if(send(socket, message, send_length, 0) < 0)
		my_err("send", __LINE__);
}

/*踢人*/
void del_member(char *str, int socket)
{
	char own_name[20] = {0};      //踢人的人名
	char name[20] = {0};          //被踢的人名
	char group_name[20] = {0};    //群名
	int sock_fd = 0;
	int i, j = 0, count = 0;
	for(i = 5; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			i++, count++;
		if(count == 0)
			group_name[j++] = str[i];
		if(count == 1)
		{
			strcpy(name, &str[i]);
			break;
		}
	}
	printf("group_name = %s name = %s\n",group_name, name);
	char *sql_se = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_se, strlen(sql_se)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		else if(socket == atoi(row[1]))
		{
			strcpy(own_name, row[0]);
			count++;
		}
		if(count == 2)
			break;
	}
	char *sql_select = "SELECT group_name, member, status from groups";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if( strcmp(group_name, row[0]) == 0 && strcmp(own_name, row[1]) == 0 && ( atoi(row[2]) == 2 || atoi(row[2]) == 1) )
		{
			char sql_del[100] = {0};
			sprintf(sql_del, "DELETE from groups where group_name='%s'&&member='%s'", group_name, name);
			if( mysql_real_query(con, sql_del, strlen(sql_del)) )
				my_err("delete", __LINE__);
			if(sock_fd > 0)
			{
				char message[200] = {0};
				sprintf(message, "#你被%s群的%s踢出了该群聊", group_name, own_name);
				if(send(sock_fd, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				return;
			}
			else
				return;
		}
	}
	char message[200] = "#人可不是你能乱踢的";
	if(send(socket, message, send_length, 0) < 0)
		my_err("send", __LINE__);
}


void recv_filename(char *str, int socket)
{
	int i, j = 0, count = 0;
	char file_name[20] = {0};         //文件名
	char send_name[20] = {0};         //发送文件的名字
	char recv_name[20] = {0};		  //接收文件的名字
	int sock_fd = 0;                      //接收用户的套接字
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
			count++, i++;
		if(count == 1)
			recv_name[j++] = str[i];
		if(count == 2)
		{
			strcpy(file_name, &str[i]);
			break;
		}
	}
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con , sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	count = 0;
	while(row = mysql_fetch_row(res))
	{
		if(socket == atoi(row[1]))
		{
			strcpy(send_name, row[0]);
			count++;
		}
		if(strcmp(recv_name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			count++;
		}
		if(count == 2)
			break;
	}
	printf("filename = %s sendname = %s recvname = %s\n", file_name, send_name, recv_name);
	char message[200] = {0};
	sprintf(message, "^:%s:%s: %s send to you a file , name is %s", send_name, file_name, send_name, file_name);
	printf("message = %s\n", message);
	if(sock_fd > 0)
	{
		if(send(sock_fd, message, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
}

/*是否同意接收*/
void agg_file(char *str, int socket)
{
	char send_name[20] = {0};
	char recv_name[20] = {0};
	char message[200] = {0};
	int i, j = 0, count = 0;
	int sock_fd;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
			i++, count++;
		if(count == 1)
			send_name[j++] = str[i];
		if(count == 2)
		{
			strcpy(recv_name, &str[i]);
			break;
		}
	}
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(send_name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			break;
		}
	}
	if(sock_fd > 0)
	{
		if(strncmp(str, "~$agree", 7) == 0)
		{
			sprintf(message, "$$:%s", recv_name);
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
		else
		{
			sprintf(message, "**:%s", recv_name);
			if(send(sock_fd, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
	}
}

/*发送文件*/
void send_file(char *str, int socket)
{
	char message[200] = "&:";
	char name[20] = {0};
	char length[4] = {0};
	char length2[20] = {0};
	int i, j = 0, count = 0;
	int sock_fd = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 4)
		{
			i++, count++;
			j = 0;
		}
		if(count == 1)
			name[j++] = str[i];
		if(count == 2)
			length[j++] = str[i];
		if(count == 3)
			length2[j++] = str[i];
		if(count == 4)
			break;
	}
	strcat(message, length);
	message[strlen(message)] = ':';
	strcat(message, length2);
	message[strlen(message)] = ':';
	j = strlen(message);
	int k = i;

	for(; i < atoi(length) + k; i++)
		message[j++] = str[i];
	printf("name = %s length = %s length2 = %s  message = %s\n", name, length, length2, message);
	sock_fd = find_online(head, name); //

	/*
	char *sql_select = "SELECT username, socket from online";
	if( mysql_real_query(con, sql_select, strlen(sql_select)) )
		my_err("select", __LINE__);
	res = mysql_store_result(con);
	while(row = mysql_fetch_row(res))
	{
		if(strcmp(name, row[0]) == 0)
		{
			sock_fd = atoi(row[1]);
			break;
		}
	}
	*/
	if(sock_fd > 0)
	{
		if(send(sock_fd, message, send_length, 0) < 0)
			my_err("send_File", __LINE__);
	}
}



int main(void)
{
	/*初始化mysql*/
	con = mysql_init(con);
	mysql_real_connect(con, host, user, key, db, 0, NULL, 0);

	pthread_mutex_init(&mutex, NULL);
	
	create(&head);

	int serv_fd, cli_fd;
	int epfd;                  //epoll句柄
	epfd = epoll_create(256);
	struct sockaddr_in serv_addr, cli_addr; //服务端客户端套接字地址
	struct epoll_event serv_ev;             //epoll事件结构体
	int cond;								//epoll_wait 的返回值	
	char readbuf[200] = {0};            //读取客户端发来的消息
	int flag;                            //判断recv返回值
	if( (serv_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		my_err("socket", __LINE__);
	/*设置服务器结构*/
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	socklen_t len = sizeof(struct sockaddr);
	if(bind(serv_fd, (struct sockaddr *)&serv_addr, len) < 0)
		my_err("bind", __LINE__);
	if(listen(serv_fd, 20) < 0)
		my_err("listen", __LINE__);
	
	/*给epoll中的事件赋值*/
	struct epoll_event max_ev[64];
	/*监听服务器*/
	serv_ev.events = EPOLLIN;
	serv_ev.data.fd = serv_fd;
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, serv_fd, &serv_ev) < 0)
		my_err("epoll_ctl", __LINE__);
	int i, fd;
	int recv_length = 200, count = 0; // 设置接收长度200， 与发送长度一致，防止数据流混乱
	pool_init(10);
	while(1)
	{	
		if( (cond = epoll_wait(epfd, max_ev, 64, -1)) < 0)
			my_err("epoll_wait", __LINE__);
		for(i = 0; i < cond; i++)
		{
			fd = max_ev[i].data.fd;
			/*接收用户登录*/
			if(fd == serv_fd)
			{
				if( (cli_fd = accept(serv_fd, (struct sockaddr *)&serv_addr, &len)) < 0)
					my_err("accept", __LINE__);
				printf("连接成功\n");
				serv_ev.data.fd = cli_fd;
				serv_ev.events = EPOLLIN;
				if(epoll_ctl(epfd, EPOLL_CTL_ADD, cli_fd, &serv_ev) < 0)
					my_err("epoll_ctl", __LINE__);
			}
			/*处理用户发送的消息*/
			else
			{
				while( (flag = recv(fd, &readbuf[count], recv_length, 0)) )
				{
					count += flag;
					if(count == 200)
					 	break;
					else
						recv_length -= flag;
				}
				/*recv 返回0 套接字断开连接，将其从句柄中删除*/
				if(flag <= 0)
				{
					printf("%d用户断开连接\n", fd);
					rm_socket(fd);
					head = del_online(head, fd);
					epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &max_ev[i]);
					close(fd);            //关闭文件描述符
				}
				else
				{
					pool_add_worker((process), readbuf, fd);
				}
			}
			recv_length = 200;
			count = 0;
			memset(readbuf, 0, sizeof(readbuf));
		}
	}
	close(serv_fd);
}
