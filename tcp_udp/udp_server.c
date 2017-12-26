#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#include <netdb.h>  
#include <net/if.h>  
#include <sys/ioctl.h>  

#define IP_SIZE     16

int get_local_ip(const char *eth_inf, char *ip);

int main()
{
	char ip[IP_SIZE];
	const char *test_eth = "wlan0";

	get_local_ip(test_eth, ip);

	char msg[128] = "network:";
	strcat(msg, ip);

	int	brdcFd;
	if ( (brdcFd = socket( PF_INET, SOCK_DGRAM, 0 ) ) == -1 )
	{
		printf( "socket fail\n" );
		return(-1);
	}

	int optval = 1; /* 这个值一定要设置，否则可能导致sendto()失败 */
	//setsockopt( brdcFd, SOL_SOCKET, SO_BROADCAST | SO_REUSEADDR, &optval, sizeof(int) );
	int ret;
	ret = setsockopt(brdcFd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(int));
	if(ret!=0)
	{
		printf("setsockopt SO_BROADCAST error:%d, %s\n", errno, strerror(errno));
		close(brdcFd);
		return -1;
	}

	ret = setsockopt(brdcFd, SOL_SOCKET, SO_REUSEADDR, &optval,sizeof(int));
	if(ret!=0)
	{
		printf("setsockopt SO_REUSEADDR error:%d, %s\n", errno, strerror(errno));
		close(brdcFd);
		return -1;
	}

	struct sockaddr_in theirAddr;
	memset( &theirAddr, 0, sizeof(struct sockaddr_in) );
	theirAddr.sin_family		= AF_INET;
	theirAddr.sin_addr.s_addr	= inet_addr( "255.255.255.255" );
	theirAddr.sin_port			= htons( 6666 );
	int sendBytes;

	while(1) {

		if ( (sendBytes = sendto( brdcFd, msg, strlen( msg ), 0,
					  (struct sockaddr *) &theirAddr, sizeof(struct sockaddr) ) ) == -1 )
		{
			printf( "sendto fail, errno=%d\n", errno );
			return(-1);
		}
		printf( "msg=%s, msgLen=%d, sendBytes=%d\n", msg, strlen( msg ), sendBytes );
		sleep(3);
	}
	
	close( brdcFd );
	return(0);
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
