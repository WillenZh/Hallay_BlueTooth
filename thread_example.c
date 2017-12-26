
/**
 * author:  张 炜
 * write by 2017/12/18
 * compile: mips-linux-gnu-gcc thread_example.c -lpthread -o thread_willen
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/wait.h>
#include <errno.h>

#include <netdb.h>  
#include <net/if.h>  
#include <sys/ioctl.h>

#include <dirent.h>

#define IP_SIZE     16
#define BT_SHELL "gatttool -b %s --char-write-req -a 0x000f -n 0100 --listen &"


/**判断str1是否以str2开头
 * 如果是返回1
 * 不是返回0
 * 出错返回-1
 * */
int is_begin_with(const char * str1,char *str2)
{
        if(str1 == NULL || str2 == NULL)
                return -1;

        int len1 = strlen(str1);
        int len2 = strlen(str2);
        
        if((len1 < len2) || (len1 == 0 || len2 == 0))
                return -1;
        
        char *p = str2;
        int i = 0;
        while(*p != '\0')
        {
                if(*p != str1[i])
                        return 0;
                p++;
                i++;
        }
        return 1;
}

/**判断str1是否以str2结尾
 * 如果是返回1
 * 不是返回0
 * 出错返回-1
 * */
int is_end_with(const char *str1, char *str2)
{
        if(str1 == NULL || str2 == NULL)
                return -1;

        int len1 = strlen(str1);
        int len2 = strlen(str2);
        
        if((len1 < len2) || (len1 == 0 || len2 == 0))
                return -1;
        
        while(len2 >= 1)
        {
                if(str2[len2 - 1] != str1[len1 - 1])
                        return 0;
                
                len2--;
                len1--;
        }
        return 1;
}


//获取本机ip
int get_local_ip(const char *eth_inf, char *ip)
{
    int sd;
    struct sockaddr_in sin;
    struct ifreq ifr;

    sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (-1 == sd)
    {
        printf("socket error: %s\n", strerror(errno));
        return -1;  
    }

    strncpy(ifr.ifr_name, eth_inf, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
 
    // if error: No such device
    if (ioctl(sd, SIOCGIFADDR, &ifr) < 0)
    {
        printf("ioctl error: %s\n", strerror(errno));
        close(sd);
        return -1;
    }
 
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    snprintf(ip, IP_SIZE, "%s", inet_ntoa(sin.sin_addr));

    close(sd);
    return 0;
}

pthread_t thread[2]; 
pthread_mutex_t mut; 
int share_resource=0;

//标识是否需要继续发送UDP数据
int needUDPData = 1;

void *thread1() 
{
        char ip[IP_SIZE];
        const char *test_eth = "wlan0";

        int     brdcFd;
        if ( (brdcFd = socket( PF_INET, SOCK_DGRAM, 0 ) ) == -1 )
        {
                printf( "udp socket fail\n" );
                pthread_exit(NULL); 
        }

        int optval = 1; /* 这个值一定要设置，否则可能导致sendto()失败 */
        //setsockopt( brdcFd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int) );
        int ret;
        ret = setsockopt(brdcFd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(int));
        if(ret!=0)
        {
                printf("setsockopt SO_BROADCAST error:%d, %s\n", errno, strerror(errno));
                close(brdcFd);
                pthread_exit(NULL); 
        }

        ret = setsockopt(brdcFd, SOL_SOCKET, SO_REUSEADDR, &optval,sizeof(int));
        if(ret!=0)
        {
                printf("setsockopt SO_REUSEADDR error:%d, %s\n", errno, strerror(errno));
                close(brdcFd);
                pthread_exit(NULL); 
        }

        struct sockaddr_in theirAddr;
        memset( &theirAddr, 0, sizeof(struct sockaddr_in) );
        theirAddr.sin_family            = AF_INET;
        theirAddr.sin_addr.s_addr       = inet_addr( "255.255.255.255" );
        theirAddr.sin_port              = htons( 6666 );
        int sendBytes;

        char msg[30] = "\0";
        while(1) {
                printf("子线程1执行UDP任务: share_resource = %d\n",share_resource);
                pthread_mutex_lock(&mut);
                share_resource++;
                pthread_mutex_unlock(&mut);

                if( needUDPData && get_local_ip(test_eth, ip) == 0 ) {
                        strcpy(msg, "network:");
                        //printf("%s", msg);
                        strcat(msg, ip);

                        if ( (sendBytes = sendto( brdcFd, msg, strlen( msg ), 0,
                                                  (struct sockaddr *) &theirAddr, sizeof(struct sockaddr) ) ) == -1 )
                        {
                                printf( "udp send fail, errno=%d\n", errno );
                        } else {
                                printf( "udp send msg=%s\n", msg);                                
                        }
                }

                sleep(3);
        }

        pthread_exit(NULL); 
}

void *thread2() 
{
        int                     server_sockfd;      /*服务器端套接字 */
        int                     client_sockfd;      /* 客户端套接字 */
        int                     len;
        struct sockaddr_in      my_addr;            /* 服务器网络地址结构体 */
        struct sockaddr_in      remote_addr;        /* 客户端网络地址结构体 */
        int                     sin_size;
        char                    buf[BUFSIZ];        /* 数据传送的缓冲区 */
        memset( &my_addr, 0, sizeof(my_addr) );     /* 数据初始化--清零 */
        my_addr.sin_family      = AF_INET;          /* 设置为IP通信 */
        my_addr.sin_addr.s_addr = INADDR_ANY;       /* 服务器IP地址--允许连接到所有本地地址上 */
        my_addr.sin_port        = htons( 8888 );    /* 服务器端口号 */

        /*创建服务器端套接字--IPv4协议，面向连接通信，TCP协议*/
        if ( (server_sockfd = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
        {
                perror( "socket error" );
                pthread_exit(NULL);
        }


        /*将套接字绑定到服务器的网络地址上*/
        if ( bind( server_sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr) ) < 0 )
        {
                perror( "bind error" );
                pthread_exit(NULL);
        }

        /*监听连接请求--监听队列长度为5*/
        if ( listen( server_sockfd, 5 ) < 0 )
        {
                perror( "listen error" );
                pthread_exit(NULL);
        }

        sin_size = sizeof(struct sockaddr_in);

        while(1) {
                printf("子线程2执行TCP任务: share_resource = %d\n",share_resource); 
                pthread_mutex_lock(&mut);
                share_resource++;
                pthread_mutex_unlock(&mut);

                /*等待客户端连接请求到达*/
                if ( (client_sockfd = accept( server_sockfd, (struct sockaddr *) &remote_addr, &sin_size ) ) < 0 )
                {
                        perror( "accept error" );
                        continue;
                }

                printf( "accept client %s\n", inet_ntoa( remote_addr.sin_addr ) );
                //len = send( client_sockfd, "connected\n", 21, 0 ); /* 发送成功信息 */
                
                //设置socket超时时间10s
                struct timeval timeout;      
                timeout.tv_sec = 10;
                timeout.tv_usec = 0;

                if (setsockopt (client_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
                {
                        perror("setsockopt SO_RCVTIMEO failed\n");
                }

                if (setsockopt (client_sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
                {
                        perror("setsockopt SO_SNDTIMEO failed\n");
                }
                
                //标识是否需要继续发送UDP数据
                needUDPData = 0;

                /*发送心跳数据给客户端并接收客户端的心跳数据--recv返回接收到的字节数，send返回发送的字节数*/
                while (1)
                {
                        /* 循环发送心跳信息 */
                        if ( send( client_sockfd, "connected\n", 10, 0 ) < 0 ) {
                                perror( "write error" );
                                break;
                        }

                        //循环接收数据
                        if((len = recv( client_sockfd, buf, BUFSIZ, 0 )) <= 0) {
                                perror( "receive error" );
                                break;
                        }

                        buf[len] = '\0';

                        //判断是否是心跳数据
                        if (strcmp(buf, "connected") == 0) {
                                printf( "心跳数据\n" );

                        } else if (is_begin_with(buf, "bluetooth") == 1) {
                                //判断是否是蓝牙配置信息
                                strtok(buf, "@");
                                char *p = strtok(NULL, "@");

                                printf( "蓝牙配置数据：%s\n", p );
                                char shell[50] = "touch /tmp/willen/bluetooth/";
                                strcat(shell, p);
                                system("rm -rf /tmp/willen/bluetooth/*");
                                printf("%s:%d\n", shell, system(shell));

                        } else if (is_begin_with(buf, "server") == 1) {
                                //判断是否是服务器接口信息
                                strtok(buf, "@");
                                char *p = strtok(NULL, "@");

                                printf( "服务器接口数据：%s\n", p );
                                char shell[50] = "touch /tmp/willen/server/";
                                strcat(shell, p);
                                system("rm -rf /tmp/willen/server/*");
                                printf("%s:%d\n", shell, system(shell));

                        } else if (is_begin_with(buf, "ip") == 1) {
                                //判断是否是静态IP信息
                                strtok(buf, "@");
                                char *p = strtok(NULL, "@");

                                printf( "静态IP数据：%s\n", p );
                                char shell[50] = "touch /tmp/willen/ip/";
                                strcat(shell, p);
                                system("rm -rf /tmp/willen/ip/*");
                                printf("%s:%d\n", shell, system(shell));

                        } else {
                                //其他数据直接打印
                                printf( "其他数据：%s\n", buf );
                        }

                        sleep(3);
                }

                //标识是否需要继续发送UDP数据
                needUDPData = 1;

                /*关闭套接字*/
                close( client_sockfd );
                sleep(3);
        }

        /*关闭套接字*/
        close( server_sockfd );
        pthread_exit(NULL);
}

void thread_create(void) 
{ 
        int temp; 
        memset(&thread, 0, sizeof(thread));

        /*创建线程*/ 
        if((temp = pthread_create(&thread[0], NULL, thread1, NULL)) != 0)
                printf("线程1创建失败!\n");
        else
                printf("线程1被创建.\n");

        if((temp = pthread_create(&thread[1], NULL, thread2, NULL)) != 0)
                printf("线程2创建失败!\n");
        else
                printf("线程2被创建.\n");

}

//判断是否存在某个文件
int existFile(char *basePath, char *fileName) {

    DIR *dir;
    struct dirent *ptr;

    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        return 1;
    }

    int i = 0;
    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0) {   ///current dir OR parrent dir
            continue;

        } else if(ptr->d_type == 8 || ptr->d_type == 10) { // 8 file 10 link file

            if(strcmp(ptr->d_name, fileName) == 0) {
                printf("存在%s文件！\n", fileName);
                //closedir(dir);
                return 0;
            }

        }
    }

    printf("不存在%s文件！\n", fileName);
    //closedir(dir);
    return 1;
}

//读取文件夹里所有的文件列表
int readFileList(char *basePath, char *filesList)
{
    DIR *dir;
    struct dirent *ptr;
    //char base[1000];

    if ((dir=opendir(basePath)) == NULL)
    {
        perror("Open dir error...");
        return 1;
    }

    int i = 0;
    while ((ptr=readdir(dir)) != NULL)
    {
        if(strcmp(ptr->d_name,".")==0 || strcmp(ptr->d_name,"..")==0) {   ///current dir OR parrent dir
            continue;

        } else if(ptr->d_type == 8 || ptr->d_type == 10) {   ///file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
            if(i==0){
                strcpy(filesList, ptr->d_name);
            }else{
                strcat(filesList, "#");
                strcat(filesList, ptr->d_name);
            }
        }

        i++;
    }

    //closedir(dir);

    if(i==0) {
        return 1;
    } else {
        return 0;
    }
}

int main() 
{
        /*用默认属性初始化互斥锁*/ 
        pthread_mutex_init(&mut,NULL);
        printf("我是主函数，我正在创建线程\n");

        thread_create();
        
        //开启蓝牙功能
        printf("我是主进程，正在开启蓝牙功能：\n");
        system("rm -rf /var/run/messagebus.pid");
        int result = system("bt_enable");
        printf("我是主进程，开启蓝牙完成:%d\n", result);
        system("rm -rf /tmp/willen/connected/*");
        sleep(3);

        //保存蓝牙设备的配置信息
        char *btData = malloc(50);
        
        //保存蓝牙连接命令
        char *shell = malloc(100);

        char *tmp = malloc(50);

        //保存蓝牙连接的文件名
        char *fileName = malloc(60);

        //保存文件状态信息的结构体
        struct stat fileStatus;

        //保存当前的时间戳
        time_t time_now;

        while(1) {
                printf("我是主进程，正在执行蓝牙连接任务...\n");

                memset(btData, 0, 50);
                memset(shell, 0, 100);

                //读取蓝牙配置的信息
                if( readFileList("/tmp/willen/bluetooth", btData) == 0 ) {

                        //读取信息成功
                        sprintf(shell, BT_SHELL, btData);
                        printf("正在连接蓝牙%s：%d\n", btData, system(shell));
                        sleep(12);

                        memset(fileName, 0, 60);
                        sprintf(fileName, "/tmp/willen/connected/%s", btData);

                        //判断连接是否成功
                        if(existFile("/tmp/willen/connected", btData) == 0) {
                                //蓝牙连接成功

                                //循环监听蓝牙是否断开连接
                                while(1) {
                                        printf("正在监听蓝牙是否断开连接...\n");
                                        
                                        stat(fileName, &fileStatus);
                                        time(&time_now);

                                        //printf("%s modify time: %d, now: %d\n", fileName, fileStatus.st_mtime, time_now);
                                        //判断是否超时15s
                                        if((time_now - fileStatus.st_mtime) >= 15) {
                                                //读取数据超时，设备已经断开
                                                
                                                //TODO 杀死蓝牙连接的相关进程
                                                memset(tmp, 0, 50);
                                                sprintf(tmp, "ps | grep %s > %s", btData, fileName);
                                                system(tmp);

                                                memset(fileName, 0, 60);
                                                sprintf(fileName, "/tmp/willen/connected/%s", btData);
                                                printf("### %s\n", fileName);
                                                FILE *fpRead=fopen(fileName,"r");
                                                if(fpRead != NULL)
                                                {
                                                        int pid=0;
                                                        fscanf(fpRead,"%d ",&pid);
                                                        printf("当前进程的PID：%d\n",pid);
                                                        //fclose(fpRead);

                                                        //杀死当前的进程
                                                        memset(tmp, 0, 50);
                                                        sprintf(tmp, "kill 9 %d", pid);
                                                        printf("结束当前的蓝牙进程: %d\n", system(tmp));

                                                } else {
                                                        perror("Error");
                                                }

                                                //删除相关文件
                                                memset(tmp, 0, 50);
                                                sprintf(tmp, "rm -rf %s", fileName);
                                                printf("%s设备已经断开: %d\n", btData, system(tmp));

                                                break;
                                        }
                                        sleep(6);
                                }

                        } else {
                                printf("蓝牙连接失败!\n");

                                //TODO 杀死蓝牙连接的相关进程
                                memset(tmp, 0, 50);
                                sprintf(tmp, "ps | grep %s > %s", btData, fileName);
                                system(tmp);

                                memset(fileName, 0, 60);
                                sprintf(fileName, "/tmp/willen/connected/%s", btData);
                                printf("### %s\n", fileName);
                                FILE *fpRead=fopen(fileName, "r");
                                if(fpRead != NULL)
                                {
                                        int pid=0;
                                        fscanf(fpRead,"%d ",&pid);
                                        printf("当前进程的PID：%d\n",pid);
                                        //fclose(fpRead);

                                        //杀死当前的进程
                                        memset(tmp, 0, 50);
                                        sprintf(tmp, "kill 9 %d", pid);
                                        printf("结束当前的蓝牙进程: %d\n", system(tmp));

                                } else {
                                        perror("Error");
                                }

                                //删除相关文件
                                memset(tmp, 0, 50);
                                sprintf(tmp, "rm -rf %s", fileName);
                                printf("%s设备已经断开: %d\n", btData, system(tmp));

                        }

                } else {
                        printf("尚未配置蓝牙设备的信息!\n");
                }

                sleep(6);
        }

        free(btData);
        free(shell);
        free(fileName);
        free(tmp);

        return 0;
}
