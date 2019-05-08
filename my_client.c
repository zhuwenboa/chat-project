#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<mysql/mysql.h>
#include<pthread.h>
#include<termios.h>
#include<assert.h>
#include<stdio_ext.h>

/*客户端*/
#define PORT 8848           //端口地址
#define send_length 200     
#define CLOSE printf("\033[0m");     //关闭彩色字体                         
#define GREEN printf("\e[1;32m");      //绿色字体
#define RED printf("\e[1;31m");        //红色字体
#define CLEAR printf("\033[2J");       //清屏
#define YELLOW printf("\e[1;33m");     //黄色字体
#define BLUE printf("\e[1;34m");       //蓝色字体

int status;                          //判断该用户是否在聊天状态
char online_name[20];                //当前用户在和谁聊天
char file_name[20];                  //发送的文件名
char own_name[20];                   //当前登录的用户名

int fd_read;                         //读取文件描述符
int fd_write;                        //写入文件描述符

typedef struct BOX
{
	char name[20];
	char message[200];
	struct BOX *next;
}box;

box *head = NULL;

/*创建消息队列*/
void create(box **head)
{
	box *p;
	p = (box *)malloc(sizeof(box));
	*head = p;
	p->next = NULL;
}

/*向消息队列中添加消息*/
box *add(box *head,	char *name, char *message)
{
	box *add = (box *)malloc(sizeof(box));
	strcpy(add->name, name);
	strcpy(add->message, message);
	box *temp = head;
	while( temp->next != NULL)
		temp = temp->next;
	temp->next = add;
	return head;
}
/*删除消息队列中的内容*/
box *del(box *head, char *name)
{
	box *pre, *rev;
	pre = NULL;
	rev = head;
	while(rev != NULL)
	{
		if(strcmp(rev->name, name) == 0)
		{
			return rev;
			pre->next = rev->next;
		}
		else
		{
			pre = rev;
			rev = rev->next;	
		}
	}
}

/*错误处理函数*/
void my_err(char *str, int line)
{
	fprintf(stderr, "line:%d\n", line);
	perror(str);
	exit(0);
}

/*getchar()的实现*/
int getch()
{
    int c=0;
    struct termios org_opts, new_opts;
    int res=0;
     res=tcgetattr(STDIN_FILENO, &org_opts);
     assert(res==0);
    memcpy(&new_opts, &org_opts, sizeof(new_opts));
    new_opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE | ICRNL);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_opts);
    c=getchar();
    res=tcsetattr(STDIN_FILENO, TCSANOW, &org_opts);
    assert(res==0);
    return c;
}

/*注册*/
int user_login(int socket)	
{
	char use[200] = {0};
	char username[21] = {0};
	char passwd[21] = {0};
	char ans[21] = {0};
	char flag[200] = {0};
	int recv_length = 200;
	printf("----------------------\n");
	printf("\t\t注册\n");
	printf("用户名长度小于20\n");
	printf("请输入要注册的用户名：");
//	getchar();
	scanf("%[^\n]", username);
	username[strlen(username)] = ':';     //分隔名字，密码，密保问题的分隔符
	username[strlen(username)] = '\0';

	printf("请输入密码，长度小于20: ");
	getchar();
	scanf("%[^\n]", passwd);
	passwd[strlen(passwd)] = ':';
	passwd[strlen(passwd)] = '\0';

	printf("密保问题: 你帅不帅\n");
	getchar();
	scanf("%[^\n]", ans);
	ans[strlen(ans)] = '\0';
	strcpy(use, "login:");
	strcat(use, username);
	strcat(use, passwd);
	strcat(use, ans);
	if(send(socket, use, send_length, 0) < 0)
		my_err("send", __LINE__);
	int len, count = 0;
	while( (len = recv(socket, flag, recv_length, 0)) )
	{
		count += len;
		if(count == 200)
			break;
		else
			recv_length -= len;
	}
	if(len == -1)
		my_err("recv", __LINE__);
	if(strncmp(flag, "success", 7) == 0)
	{
		printf("注册成功\n");
		printf("----------------------\n");
		return 1;
	}
	else
		return 0;
}
/*登录*/
int user_enter(int socket)
{
	char use[200] = "enter:";
	char name[21] = {0};					//用户名
	char passwd[20] = {0};					//密码
	char flag[200] = {0};                        
	int recv_length = 200;      
	printf("------------------\n");
	printf("登录界面\n");
	printf("请输入你的用户名：");
	scanf("%[^\n]", name);
	strcpy(own_name, name);                  //保存当前登录用户的名字
	name[strlen(name)] = ':';
	name[strlen(name)] = '\0';
	printf("请输入你的密码：");
	getchar();
	int i = 0;
	int ch;
	ch = getch();
	while(ch != '\n')
	{
		if(ch == 127)
		{
			passwd[--i] = '\0';
			printf("\b \b");
			ch = getch();
			continue;
		}
		passwd[i++] = ch;
		printf("*");
		ch = getch();
	}
	printf("\n");
	//printf("passwd = %s \n", passwd);
	strcat(use, name);
	strcat(use, passwd);
	if(send(socket, use, send_length, 0) < 0)
		my_err("send", __LINE__);
	int len, count = 0;
	while( (len = recv(socket, flag, recv_length, 0)) )
	{
		count += len;
		if(count == 200)
			break;
		else
			recv_length -= len;
	}
	if(len == -1)
		my_err("recv", __LINE__);
	if(strncmp(flag, "success", 7) == 0)
	{
		printf("登录成功\n");
		printf("------------------\n");
		return 1;
	}
	else
		return 0;
}
//找回密码
int find_passwd(int socket)
{
	char answer[200] = "find_passwd:";
	char name[21] = {0};								//用户名
	char ans[21] = {0};									//回答
	char passwd[200] = {0};                    		    //找回的密码
	printf("-------------------\n");
	printf("找回密码\n");
	printf("请输入你的用户名\n");
//	getchar();
	scanf("%[^\n]", name);
	name[strlen(name)] = ':';
	name[strlen(name)] = '\0';
	printf("问题：你帅不帅\n");
	getchar();
	scanf("%[^\n]", ans);
	/*发送请求及用户名和答案*/
	strcat(answer, name);           
	strcat(answer, ans);		
	if(send(socket, answer, send_length, 0) < 0)
		my_err("send", __LINE__);
	int len, count = 0;
	int recv_length = 200;
	while( (len = recv(socket, passwd, recv_length, 0)) )
	{
		count += len;
		if(count == 200)
			break;
		else
			recv_length -= len;
	}
	if(len == -1)
		my_err("recv", __LINE__);
	if(strncmp(passwd, "fault", 5))
	{
		//strcpy(passwd, flag);
		printf("回答正确，您的密码是：%s\n", passwd);
		printf("---------------\n");
		return 1;
	}
	else
		return 0;
}
/*是否添加好友*/
void add_friend(char *str, int socket)
{
	printf("str = %s\n", str);
	int count = 0, j = 0;
	char name[20] = {0};
	char message[200] = {0};
	for(int i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
		{
			count++;
			i++;
		}
		if(count == 1)	
			name[j++] = str[i];
		
		if(count == 2)
		{
			strcpy(message, name);
			strcat(message, &str[i]);
			break;
		}	
	}
	printf("好友添加请求 %s\n", message);
	printf("是否同意: y/n\n");
	char flag;
	getchar();
	scanf("%c", &flag);
	if(flag == 'y')
	{
		char n[50] = "~@agree:";
		strcat(n, name);
		printf("n = %s\n", n);
		if(send(socket, n, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
	else
	{
		char n[50] = "~@disagree:";
		strcat(n, name);
		if(send(socket, n, send_length, 0) < 0)
			my_err("send", __LINE__);
	}
	
}

/*查看好友状态信息*/
void watch_friend(char *str)
{
	int i, j = 0, count = 0;
	int len = 0;
	char name[200] = {0};
	char message[200] = {0};
	int flag;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':')
		{
			count++;
			i++;
		}
		if(str[i] == ' ')
		{
			count++;
			i++;
		}
		if(count == 1)
		{
			name[j++] = str[i];               //获取用户名
		}
		if(count == 2)
		{
			len = strlen(name);
			for(j = 0; j < 30 - len; j++)
				message[j] = ' ';
			if(strcmp(&str[i], "online") == 0)
				flag = 0;
			else
				flag = 1;
			strcat(message, &str[i]);
			break;
		}
	}
	if(flag == 0)
	{
		printf("\t%s", name);
		GREEN
		printf("%s\n", message);
		CLOSE
	}
	else
	{
		printf("\t%s", name);
		RED
		printf("%s\n", message);
		CLOSE
	}
}

/*接收离线消息*/
void recv_record(char *str)
{
	int i, j = 0, count = 0;
	char message[200] = {0};
	for(i = 0; str[i] != 0; i++)
	{
		if(str[i] == ':')
			count++, i++;
		if(count == 1)
			message[j++] = str[i];
	}
	YELLOW
	printf("您有离线消息: %s\n", message);
	CLOSE
}

/*查看历史记录*/
void recv_history(char *str)
{
	int i = 0, j = 0, count = 0;
	char message[200] = {0};
	for(; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			message[j++] = str[i];
	}
	printf("%s\n", message);
}

/*创建群聊*/
void create_group(int socket)
{
	int len = 0, count = 0, recv_length = 200;
	printf("\t   创建群聊 \n");
	printf("请输入想要创建的群名字,长度小于20: ");
	char name[20] = {0};
	char recv_buf[200] = {0};                         //接收服务器返回的消息
	getchar();
	scanf("%[^\n]", name);
	char message[200] = "!create:";
	strcat(message, name);
	printf("create message = %s\n", message);
	if(send(socket, message, send_length, 0) < 0)
		my_err("send", __LINE__);
	while(len = recv(socket, recv_buf, recv_length, 0) < 0)
	{
		if(count == 200)
			break;
		count += len;
		recv_length -= len;
	}
	if(strncmp(recv_buf, "success", 7) == 0)
		printf("恭喜你创建成功\n");
	else
		printf("创建失败\n");
}

/*加群*/
void add_group(char *str, int socket)
{
	char message2[100] = {0};
	char group[20] = {0};
	int i, j = 0, count = 0;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 1)
			count++, i++;
		if(count == 1)
			message2[j++] = str[i];
	}
	printf("\t%s\n", message2);
	printf("1 同意\n");
	printf("2 不同意\n");
	char flag[2];
	while(1)
	{
		scanf("%s", flag);
		if(strcmp(flag, "1") == 0)
		{
			char message[200] = "~@@ag:";
			char name[50] = {0};
			printf("请输入群名:加用户名\n");
			scanf("%s", name);
			strcat(message, name);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
			break;
		}
		else if(strcmp(flag, "2") == 0)
		{
			char message[200] = "@@dis:";
			char name[50] = {0};
			printf("请输入群名:加用户名\n");
			scanf("%s", name);
			strcat(message, name);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
			break;
		}
	}
}

/*查看历史记录*/
void g_his(char *str)
{
	char message[200] = {0};
	strcpy(message, &str[6]);
	printf("%s\n", message);
}

/*查看群成员*/
void watch_member(char *str)
{
	int i, j = 0, count = 0;
	char name[20] = {0};
	char status[50] = {0};
	char message[30] = {0};
	for(i = 7; str[i] != '\0'; i++)
	{
		if(str[i] == ':' && count < 2)
			count++, i++;
		if(count == 1)
			name[j++] = str[i];
		if(count == 2)
		{
			strcpy(status, &str[i]);
			break;
		}
	}
	int len;
	if(strcmp(status, "online") == 0)
	{
		printf("%s", name);
		len = strlen(name);
		for(j = 0; j < 30 - len; j++)
			message[j] = ' ';
		GREEN
		
		printf("%s%s\n",message,status);
		CLOSE
	}
	else
	{
		printf("%s", name);
		len = strlen(name);
		for(j = 0; j < 30 - len; j++)
			message[j] = ' ';
		RED
		printf("%s%s\n",message, status);
		CLOSE
	}
}

/*查看所加入的群*/
void w_group(char *str)
{
	char group_name[20] = {0};
	strcpy(group_name, &str[2]);
	printf("群名%s\n", group_name);
}

/*传输文件*/
void send_file(int socket)
{
	char message1[200] = "~name:";
	char name[20];
	printf("输入要发送的对象: ");
	scanf("%s", name);
	strcat(message1, name);
	strcat(message1, ":");
	printf("输入发送的文件名：");
	scanf("%s", file_name);
	strcat(message1, file_name);
	printf("message = %s\n", message1);
	if(send(socket ,message1, send_length, 0) < 0)
		my_err("send", __LINE__);
}


/*发消息*/
void *send_message(void *arg)
{
	int socket = *(int *)arg; 
	char q;
	while(1)
	{
		system("clear");
		printf("1 好友管理\n");
		printf("2 群管理\n");
		printf("3 文件发送\n");
		printf("4 消息通知管理\n");
		printf("q 退出\n");
		//getchar();
		scanf("%c", &q);
		while(q == '1')
		{
			//system("clear");
			__fpurge(stdin);
			printf("1 私聊\n");
			printf("2 添加好友\n");
			printf("3 删除好友\n");
			printf("4 查看好友列表\n");
			printf("5 查看历史记录\n");
			printf("6 屏蔽好友输入\n");
			printf("q 退出\n");

			//getchar();

			char s;
			scanf("%c", &s);
			if(s == '1')
			{
				system("clear");
				char message[200] = "~";
				printf("请输入想选择的聊天对象\n");
				char name[20] = {0};
				scanf("%s", name);
				strcpy(online_name, name); //当前用户聊天的对象
				printf("online_name = %s\n", online_name);
				strcat(message, name);
				strcat(message, ":");
				printf("输入q退出\n");
				BLUE
				printf("--------------------聊天界面--------------------\n");
				CLOSE
				while(1)
				{
					char send_message[200] = {0};
					char send_temp[200] = {0};
					__fpurge(stdin);

					//getchar();

					scanf("%[^\n]", send_temp);
					if(strcmp(send_temp, "q") == 0)
						break;
					strcpy(send_message, message);
					strcat(send_message, send_temp);
			//		printf("message = %s\n", send_message);
					if(send(socket, send_message, send_length, 0) < 0)
						my_err("send", __LINE__);
					memset(send_message, 0, sizeof(send_message));
				}
				memset(online_name, 0, sizeof(online_name));//退出聊天将其清空
				system("clear");
			}
			else if(s == '2')
			{
				char message[200] = "~add:";
				printf("请输入好友名字: ");
				char name[20] = {0};
				scanf("%s", name);
				strcat(message, name);
				printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}
			else if(s == '3')
			{
				char message[200] = "~rm:";
				char name[20] = {0};
				printf("请输入删除好友名字：");
				scanf("%s", name);
				strcat(message, name);
				printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}
			
			else if(s == '4')
			{
				system("clear");
				char message[200] = "~";
				BLUE
				printf("-----------------好友列表-------------------\n");
				CLOSE
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				usleep(50000);
			}
			else if(s == '5')
			{
				char message[200] = "~history:";
				printf("请输入想要查看记录的姓名: ");
				__fpurge(stdin);
				//getchar();
				char name[20] = {0};
				scanf("%[^\n]", name);
				strcat(message, name);
				system("clear");
				BLUE
				printf("-------------与%s的聊天记录为-----------\n", name);
				CLOSE
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				usleep(50000);
			}
			else if(s == '6')
			{
				char message[200] = "~shield:";
				printf("请输入想要屏蔽好友的名字 :");
				__fpurge(stdin);

				//getchar();

				char name[20] = {0};
				scanf("%[^\n]", name);
				strcat(message, name);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}
			
			else if(s == 'q')
				break;
			else
			{
				__fpurge(stdin);
				system("clear");
			}
			
		}
		while(q == '2')
		{
			//system("clear");
			printf("1 创建群聊\n");
			printf("2 加入群聊\n");
			printf("3 删除群聊\n");
			printf("4 发送群消息\n");
			printf("5 查看群消息记录\n");
			printf("6 查看所加入的群\n");
			printf("7 查看群成员\n");
			printf("8 设置管理员\n");
			printf("9 踢出群成员\n");
			printf("q 退出\n");
			__fpurge(stdin);
	//		getchar();

			char w;
            scanf("%c", &w);
			if(w == '1')
			{
				create_group(socket);
			}
			else if(w == '2')
			{
				char message[200] = "~add_g:";
				printf("请输入想要添加群的名称: ");
				char group_name[20] = {0};
				__fpurge(stdin);
			//	getchar();
				scanf("%[^\n]", group_name);
				strcat(message, group_name);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}	
			else if(w == '3')
			{
				char message[200] = "~rm_group:";
				char group_name[20] = {0};
				printf("请输入想要解散群的名称：");
				__fpurge(stdin);
			//	getchar();
				scanf("%[^\n]", group_name);
				strcat(message, group_name);
				if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
				system("clear");
			}
				//usleep(50000);
			else if(w == '4')
			{
				system("clear");
				char message[200] = "~g:";
				char group_name[20] = {0};
				char send_message[200] = {0};
				char send_temp[200] = {0};
				printf("请输入想要聊天的群名称: ");
				__fpurge(stdin);
//				getchar();
				scanf("%[^\n]", group_name);
				strcpy(online_name, group_name); //正在聊天的群名
				strcat(message, group_name);
				strcat(message, ":");
				printf("输入q退出群聊\n");
				BLUE
				printf("-------------------------聊天界面--------------------\n");
				CLOSE
				while(1)
				{
					strcpy(send_message, message);
	//				getchar();
					__fpurge(stdin);
					scanf("%[^\n]", send_temp);
					if(strcmp(send_temp, "q") == 0)
						break;
					strcat(send_message, send_temp);
					if(send(socket, send_message, send_length, 0) < 0)
						my_err("send", __LINE__);
					memset(send_temp, 0, sizeof(send_temp));
					memset(send_message, 0, sizeof(send_message));
				}
				memset(online_name, 0, sizeof(online_name));
				system("clear");
			}
			else if(w == '5')
			{
				char message[200] = "~g_his:";
				char group_name[20] = {0};
				printf("请输入你想要看得群名: ");
				scanf("%s", group_name);
				strcat(message, group_name);
				system("clear");
				BLUE
				printf("---------------群%s的消息记录为-------------------\n", group_name);
				CLOSE
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				usleep(50000);
			}
			else if(w == '6')
			{
				char message[200] = "w";
				system("clear");
				BLUE
				printf("----------------加入的群聊为-------------------\n");
				CLOSE
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				usleep(10000);
			}
			else if(w == '7')
			{
				char message[200] = "~w:";
				printf("输入想看的群名称 ");
				char group_name[20] = {0};
				scanf("%s", group_name);
				strcat(message, group_name);
				system("clear");
				BLUE
				printf("-----------------------群%s的成员为-----------------\n", group_name);
				CLOSE
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				usleep(50000);	
			}
			else if(w == '8')
			{
				char message[200] = "~set:";
				char group_name[20] = {0};
				char name[20] = {0};
				printf("输入设置的群名称: ");
				scanf("%s", group_name);
				strcat(message, group_name);
				strcat(message, ":");
				printf("输入设置的用户名：");
				scanf("%s", name);
				strcat(message, name);
			//	printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}
			else if(w == '9')
			{
				char message[200] = "~del:";
				char group_name[20] = {0};
				char name[20] = {0};
				printf("输入设置的群名称: ");
				scanf("%s", group_name);
				strcat(message, group_name);
				strcat(message, ":");
				printf("输入设置的用户名：");
				scanf("%s", name);
				strcat(message, name);
	//			printf("message = %s\n", message);
				if(send(socket, message, send_length, 0) < 0)
					my_err("send", __LINE__);
				system("clear");
			}
			else if(w == 'q')
				break;
			else
			{
				__fpurge(stdin);
				system("clear");
			}
		}
		if(q == '3')
		{
			char message[200] = {0};;
			printf("请输入要发送的用户名：");
			char name[20];
			scanf("%s", name);
			printf("请输入要发送的文件名: ");
			char file[20];
			scanf("%s", file);
			strcpy(file_name, file);
			sprintf(message, "~name:%s:%s", name, file);
	//		printf("message = %s\n", message);
			if(send(socket, message, send_length, 0) < 0)
				my_err("send", __LINE__);
		}
		else if(q == '4')
		{	
			box *temp;
			temp = (box *)malloc(sizeof(box));
			box *pre = head;
			box *rev = head->next;
			while(rev != NULL)
			{
				if(strcmp(rev->name, own_name) == 0)
				{
					temp = rev;
					pre->next = rev->next;
					rev = rev->next;
				}
				else
				{
					pre = rev;
					rev = rev->next;
					continue;
				}
				if(strncmp(temp->message, "^:", 2) == 0)
				{
					char str[200];
					char file[20] = {0};
					char user_name[20] = {0};
					char message[200] = {0};
					strcpy(str, temp->message);
					int i, j = 0, count = 0;
					for(i = 0; str[i] != '\0'; i++)
					{
						if(str[i] == ':')
						{
							count++;
							i++;
							j = 0;
						}
						if(count == 1)
							user_name[j++] = str[i];
						if(count == 2)
							file[j++] = str[i];
						if(count == 3)
						{
							strcpy(message, &str[i]);
							break;
						}
					}
					printf("%s\n", message);
					printf("是否同意接收 y/n: ");
					char flag;
					while(1)
					{
						__fpurge(stdin);
					//	getchar();
						scanf("%c", &flag);
						if(flag == 'y')
						{
							char message[200] = "~$agree:";
							strcat(message, user_name);
							strcat(message, ":");
							strcat(message, own_name);
							strcpy(file_name, file);
						//	printf("message = %s\n", message);
							if(creat(file_name, 0644) < 0)
								my_err("creat_file", __LINE__);
							if( (fd_write = open(file_name, O_RDWR)) < 0)
								my_err("write", __LINE__);
							if(send(socket, message, send_length, 0) < 0)
								my_err("send", __LINE__);
							break;	
						}
						else if(flag == 'n')
						{
							char message[200] = "~$dis:";
							strcat(message, user_name);
							strcat(message, ":");
							strcat(message, own_name);
						//	printf("message = %s\n", message);
							if(send(socket, message, send_length, 0) < 0)
								my_err("send", __LINE__);
							break;
						}
						else
							printf("请输入正确的命令\n");
					}
				}
				else if(strncmp(temp->message, "add_group", 9) == 0)
				{
					add_group(temp->message, socket);
				}
				else if(strncmp(temp->message, "add", 3) == 0)
				{
					add_friend(temp->message, socket);
				}
				else
				{
					if(strncmp(temp->message, "群", 2) == 0)
					{
						char str[200];
						strcpy(str, temp->message);
						int i, j = 0, count = 0;
						char group_name[20] = {0};
						char message[200] = "~g:";
						for(i = 0; str[i] != '\0'; i++)
						{
							if(str[i] == ':')
								count++, i++;
							if(count == 1)
								group_name[j++] = str[i];
							if(count == 2)
								break;
						}
						strcpy(online_name, group_name);
						strcat(message, group_name);
						printf("1 回复\n");
						printf("2 不予理踩\n");
						__fpurge(stdin);
					//	getchar();
						char flag;
						scanf("%c", &flag);
						if(flag == '1')
						{
							system("clear");
							printf("输入消息\n");
							printf("q退出\n");
							BLUE
							printf("---------------------------聊天界面-----------------\n");
							CLOSE
							while(1)
							{
								char send_message[200] = {0};
								char temp[200] = {0};
								strcpy(send_message, message);
								getchar();
								scanf("%[^\n]", temp);
								if(strcmp(temp, "q") == 0)
									break;
								strcat(send_message, ":");
								strcat(send_message, temp);
								if(send(socket, send_message, send_length, 0) < 0)
									my_err("send", __LINE__);
							}
						}
						if(flag == '2')
							break;
						
					}
					else
					{
						while(1)
						{
							int i, j = 0, count = 0;
							char name[20] = {0};
							char message[200] = {0};
							char str[200] = {0};
							strcpy(str, temp->message);
							for(i = 0; str[i] != '\0'; i++)
							{
								if(str[i] == ':' && count < 1)
									count++, i++;
								if(count == 0)
									name[j++] = str[i];
								if(count == 1)
								{
									strcpy(message, &str[i]);
									break;
								}
							}
							strcpy(online_name, name);
							printf("%s发来了一条消息%s\n", name ,message);
							printf("1 回复\n");
							printf("2 不予理踩\n");
							__fpurge(stdin);
				//			getchar();
							char flag;
							scanf("%c", &flag);
							if(flag == '1')
							{
								system("clear");
								char message[200] = {0};
								char send_message[200] = {0};
								char temp1[200] = {0};
								sprintf(message, "~%s:", name);
								printf("发送消息\n");
								printf("q退出\n");
								BLUE
								printf("---------------------------聊天界面-----------------\n");
								CLOSE
								while(1)
								{
									__fpurge(stdin);
							//		getchar();
									scanf("%[^\n]", temp1);
									if(strcmp(temp1, "q") == 0)
									{
										flag = '2';
										break;
									}
									strcpy(send_message, message);
									strcat(send_message, temp1);
									if(send(socket, send_message, send_length, 0) < 0)
										my_err("send", __LINE__);
								}
								memset(online_name, 0, sizeof(online_name));
							}
							if(flag == '2')
								break;
						}
					}
				}
			}	
		}
		else if(q =='q')
			exit(0);
		else
			__fpurge(stdin);
		
	}
}

void *recv_message(void *arg)
{
	int socket = *(int *)arg;
	char readbuf[200] = {0};
	int i, j, k;
	char name[20] = {0};
	char lastname[20] = {0};
	int len, count;
	int recv_length;
	while(1)
	{	
		len =  count = 0;
     	recv_length = 200;
		while( (len = recv(socket, &readbuf[count], recv_length, 0)) )
		{
			count += len;
			if(count == 200)
				break;
			else
				recv_length -= len;
		}
		if(len == -1)
			my_err("recv", __LINE__);
		//readbuf[strlen(readbuf)] = '\0';
		/*添加群聊*/
		if(strncmp(readbuf, "add_group", 9) == 0)
		{
			printf("您有一条通知，请及时查看\n");
			/*添加到消息队列中*/
			head = add(head, own_name, readbuf);
		}
		/*添加好友*/
		else if(strncmp(readbuf, "add", 3) == 0)
		{
			printf("您有一条消息通知，请及时查看\n");
			/*添加到消息队列中*/
			head = add(head, own_name, readbuf);
		}
		/*接收通知消息*/
		else if(strncmp(readbuf, "#", 1) == 0)          //通知
		{
			BLUE
			printf("通知: %s\n", &readbuf[1]);
			CLOSE
		}
		/*查看好友列表*/
		else if(strncmp(readbuf, "@friend:", 8) == 0)
			watch_friend(readbuf);
		/*接收离线消息*/
		else if(strncmp(readbuf, "off:", 4) == 0)
			recv_record(readbuf);
		/*查看历史记录*/
		else if(strncmp(readbuf, "history:", 8) == 0)
			recv_history(readbuf);
		/*查看所加入的群*/
		else if(strncmp(readbuf, "g:", 2) == 0)
			w_group(readbuf);
			
		/*查看群历史记录*/
		else if(strncmp(readbuf, "g_his:", 6) == 0)
			g_his(readbuf);
		/*查看群成员*/
		else if(strncmp(readbuf, "~member:", 8) == 0)
			watch_member(readbuf);
		/*同意发送文件*/
		else if(strncmp(readbuf, "$$:", 3) == 0)
		{
			int fd;
			if( (fd = open(file_name, O_RDONLY)) < 0)
				my_err("open", __LINE__);
			char name[20];
			strcpy(name, &readbuf[3]);
			char message[200] = {0};
			char recv_message[200] = {0};
			char send_message[200] = {0};
			sprintf(message, "&:%s:", name);
			int len = strlen(message);
			int flag;
			int i, j;
			int count = 0;
			
			int fd2;
			if( (fd2 = open("text1.jpg", O_RDWR | O_CREAT, 0644)) < 0)
				my_err("open", __LINE__);
			while(1)
			{
				strcpy(send_message, message);
				if( (flag = read(fd, recv_message, 176-len)) < 1 )
					break;
				else
				{
					sprintf(&send_message[len], "%d:%d:", flag, count);
					int p ;
					p = j = strlen(send_message);
//					printf("j = %d\n", j);
					for(i = 0; i < flag; i++)
						send_message[j++] = recv_message[i];
					
					

					if(send(socket, send_message, send_length, 0) < 0)
					{
						my_err("send", __LINE__);
					}
				//	usleep(10000);
				count += flag;
				}
				memset(recv_message, 0, sizeof(recv_message));
				memset(send_message, 0, sizeof(send_message));
			}
			close(fd);
		}
		/*接收文件内容*/
		else if(strncmp(readbuf, "&:", 2) == 0)
		{
			int i, j = 0, count = 0;
			char write_message[200] = {0};
			char length[4] = {0};
			char length2[20] = {0};
			for(i = 2; readbuf[i] != '\0'; i++)
			{
				if(readbuf[i] == ':' && count < 2)
				{
					i++, count++;
					j = 0;
				}
				if(count == 0)
					length[j++] = readbuf[i];
				if(count == 1)
					length2[j++] = readbuf[i];
				if(count == 2)
					break;
			}
		if(lseek(fd_write, atoi(length2), SEEK_SET) < 0)
			my_err("lseek", __LINE__);
		
		if( write(fd_write, &readbuf[i], atoi(length)) < 0)
			my_err("write", __LINE__);
		}
		/*拒绝接收*/
		else if(strncmp(readbuf, "**", 2) == 0)
		{
			char name[20];
			strcpy(name, &readbuf[3]);
			BLUE
			printf("%s refuse your file\n", name);
			CLOSE
		}
		/*发送群消息*/
		else if(strncmp(readbuf, "群", 2) == 0)
		{
			char group_name[20] = {0};
			int i, j = 0, count = 0;
			for(i = 0; readbuf[i] != '\0'; i++)
			{
				if(readbuf[i] == ':')
					count++, i++;
				if(count == 1)
					group_name[j++] = readbuf[i];
				if(count == 2)
					break;
			}
			if(strcmp(group_name, online_name) == 0)
			{
				printf("\t%s\n", readbuf);
			}
			else
			{
				printf("%s向你发来了一条消息, 去消息通知界面进行查看\n", group_name);
				/*添加到消息队列中*/
				head = add(head, own_name, readbuf);
			}
		}
		else if(strncmp(readbuf, "^:", 2) == 0)
		{
			printf("有人给你发送一个文件，进入消息通知进行接收\n");
			/*添加到消息队列*/
			head = add(head, own_name, readbuf);
		}
		/*私聊*/
		else
		{
			char name[20] = {0};
			int i, j = 0, count = 0;
			for(i = 0; readbuf[i] != '\0'; i++)
			{
				if(readbuf[i] == ':' && count < 1)
				{
					count++, i++;
					j = 0;
				}
				if(count == 0)
					name[j++] = readbuf[i];
			}
			if(strcmp(name, online_name) == 0)
				printf("\t\t\t%s\n", readbuf);
			else
			{
				printf("%s向你发来了一条消息, 去消息通知界面进行查看\n", name);
				/*添加到消息队列中*/
				head = add(head, own_name, readbuf);
			}
		}
		memset(readbuf, 0, sizeof(readbuf));
	}
}

int main(int argc, char *argv[])
{
	/*创建消息队列*/
	create(&head);
	if(argc != 2)
		my_err("input erro", __LINE__);
	int cli_fd;
	int status;
	char message[1024] = {0};
	char message1[1024] = {0};
	struct sockaddr_in serv_addr;
	socklen_t len;
	int flag;
	if( (cli_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		my_err("socket", __LINE__);

	/*设置连接的结构*/
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	len = sizeof(struct sockaddr_in);
	if(connect(cli_fd, (struct sockaddr *)&serv_addr, len) < 0)
		my_err("connect", __LINE__);
	char select;
	while(1)
	{
		printf("------------------------------------------\n");
		printf("           welcome to qq chat             \n");
		printf("请输入1(注册)，2(登录), 3(找回密码)\n");
		scanf("%c", &select);
		__fpurge(stdin);
//		getchar();
		system("clear");
		if(select == '1')
		{
			while(!user_login(cli_fd))
				printf("注册失败，请重新注册\n");
			__fpurge(stdin);	
//			getchar();
			while(!user_enter(cli_fd))	
				printf("登录失败请重新登录\n");
			break;
		}
		else if(select == '2')
		{
			while(!user_enter(cli_fd))
				printf("登录失败，请重新登录\n");
			break;
		}
		else if(select == '3')
		{
			while(!find_passwd(cli_fd))
				printf("查找失败，请重新查找\n");
			__fpurge(stdin);
//			getchar();
			while(!user_enter(cli_fd))	
				printf("登录失败请重新登录\n");
			break;
		}
		else
			printf("小伙子请输入正确的命令\n");
	}


	pthread_t tid1, tid2;
	pthread_create(&tid1, NULL, send_message, (void *)&cli_fd);
	pthread_create(&tid2, NULL, recv_message, (void *)&cli_fd);
	pthread_join(tid1, (void *)&status);
}


