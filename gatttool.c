/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010  Nokia Corporation
 *  Copyright (C) 2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <glib.h>
#include <stdlib.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/uuid.h>

#include "att.h"
#include "btio.h"
#include "gattrib.h"
#include "gatt.h"
#include "gatttool.h"

//add by willen
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#include <dirent.h>

#define MY_HTTP_DEFAULT_PORT 80
#define MY_HTTP_DEFAULT_URL "http://%s/INurseLite/device/infusionAlert.alensic?mac_addr=%s&data=%02x&name=Kfree"

//保存服务器接口地址的信息
char *serverData = NULL;

char * http_get(const char *url);
char * http_post(const char *url,const char * post_str);
char * sendData(gchar *address, uint8_t values);

#define BUFFER_SIZE 1024
#define HTTP_POST "POST /%s HTTP/1.1\r\nHOST: %s:%d\r\nAccept: */*\r\nContent-Type:application/x-www-form-urlencoded\r\nContent-Length: %d\r\n\r\n%s"
#define HTTP_GET "GET /%s HTTP/1.1\r\nHOST: %s:%d\r\nAccept: */*\r\n\r\n"

static int http_tcpclient_create(const char *host, int port){
    struct hostent *he;  
    struct sockaddr_in server_addr;   
    int socket_fd;  
  
    if((he = gethostbyname(host))==NULL){  
        return -1;  
    }  
  
   server_addr.sin_family = AF_INET;  
   server_addr.sin_port = htons(port);  
   server_addr.sin_addr = *((struct in_addr *)he->h_addr);  
  
    if((socket_fd = socket(AF_INET,SOCK_STREAM,0))==-1){  
        return -1;  
    }  
    
    //设置socket超时时间10s
    struct timeval timeout;
    timeout.tv_sec  = 5;
    timeout.tv_usec = 0;

    if (setsockopt (socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
            perror("setsockopt SO_RCVTIMEO failed\n");
    }

    if (setsockopt (socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
            perror("setsockopt SO_SNDTIMEO failed\n");
    }

    if(connect(socket_fd, (struct sockaddr *)&server_addr,sizeof(struct sockaddr)) == -1){  
        return -1;  
    }

    return socket_fd;
}  
  
static void http_tcpclient_close(int socket){  
    close(socket);  
}  
  
static int http_parse_url(const char *url,char *host,char *file,int *port)  
{  
    char *ptr1,*ptr2;  
    int len = 0;  
    if(!url || !host || !file || !port){  
        return -1;  
    }  
  
    ptr1 = (char *)url;  
  
    if(!strncmp(ptr1,"http://",strlen("http://"))){  
        ptr1 += strlen("http://");  
    }else{  
        return -1;  
    }  
  
    ptr2 = strchr(ptr1,'/');  
    if(ptr2){  
        len = strlen(ptr1) - strlen(ptr2);  
        memcpy(host,ptr1,len);  
        host[len] = '\0';  
        if(*(ptr2 + 1)){  
            memcpy(file,ptr2 + 1,strlen(ptr2) - 1 );  
            file[strlen(ptr2) - 1] = '\0';  
        }  
    }else{  
        memcpy(host,ptr1,strlen(ptr1));  
        host[strlen(ptr1)] = '\0';  
    }  
    //get host and ip  
    ptr1 = strchr(host,':');  
    if(ptr1){  
        *ptr1++ = '\0';  
        *port = atoi(ptr1);  
    }else{  
        *port = MY_HTTP_DEFAULT_PORT;  
    }  
  
    return 0;  
}  
  
  
static int http_tcpclient_recv(int socket,char *lpbuff){  
    int recvnum = 0;  
  
    recvnum = recv(socket, lpbuff,BUFFER_SIZE*4,0);  
  
    return recvnum;  
}  
  
static int http_tcpclient_send(int socket,char *buff,int size){  
    int sent=0,tmpres=0;  
  
    while(sent < size){  
        tmpres = send(socket,buff+sent,size-sent,0);  
        if(tmpres == -1){  
            return -1;  
        }  
        sent += tmpres;  
    }  
    return sent;  
}  
  
static char *http_parse_result(const char*lpbuf)  
{  
    char *ptmp = NULL;   
    char *response = NULL;  
    ptmp = (char*)strstr(lpbuf,"HTTP/1.1");  
    if(!ptmp){
        printf("http/1.1 not faind\n");
        return NULL;
    }
    if(atoi(ptmp + 9)!=200){  
        printf("result:\n%s\n",lpbuf);  
        return NULL;  
    }  
  
    ptmp = (char*)strstr(lpbuf,"\r\n\r\n");  
    if(!ptmp){  
        printf("ptmp is NULL\n");  
        return NULL;  
    }  
    response = (char *)malloc(strlen(ptmp)+1);  
    if(!response){  
        printf("malloc failed \n");  
        return NULL;  
    }  
    strcpy(response,ptmp+4);  
    return response;
}  

char * http_post(const char *url,const char *post_str){  
  
    char post[BUFFER_SIZE] = {'\0'};  
    int socket_fd = -1;  
    char lpbuf[BUFFER_SIZE*4] = {'\0'};  
    char *ptmp;  
    char host_addr[BUFFER_SIZE] = {'\0'};  
    char file[BUFFER_SIZE] = {'\0'};  
    int port = 0;  
    int len=0;  
    char *response = NULL;  
  
    if(!url || !post_str){  
        printf("      failed!\n");  
        return NULL;  
    }  
  
    if(http_parse_url(url,host_addr,file,&port)){  
        printf("http_parse_url failed!\n");  
        return NULL;  
    }  
    //printf("host_addr : %s\tfile:%s\t,%d\n",host_addr,file,port);  
  
    socket_fd = http_tcpclient_create(host_addr,port);  
    if(socket_fd < 0){  
        printf("http_tcpclient_create failed\n");  
        return NULL;  
    }  
       
    sprintf(lpbuf,HTTP_POST,file,host_addr,port,strlen(post_str),post_str);  
  
    if(http_tcpclient_send(socket_fd,lpbuf,strlen(lpbuf)) < 0){  
        printf("http_tcpclient_send failed..\n");  
        return NULL;  
    }  
    //printf("发送请求:\n%s\n",lpbuf);  
  
    /*it's time to recv from server*/  
    if(http_tcpclient_recv(socket_fd,lpbuf) <= 0){  
        printf("http_tcpclient_recv failed\n");  
        return NULL;  
    }  
  
    http_tcpclient_close(socket_fd);  
  
    return http_parse_result(lpbuf);  
}
  
char * http_get(const char *url)  
{  
  
    char post[BUFFER_SIZE] = {'\0'};  
    int socket_fd = -1;  
    char lpbuf[BUFFER_SIZE*4] = {'\0'};  
    char *ptmp;  
    char host_addr[BUFFER_SIZE] = {'\0'};  
    char file[BUFFER_SIZE] = {'\0'};  
    int port = 0;  
    int len=0;  
  
    if(!url){  
        printf("      failed!\n");  
        return NULL;  
    }  
  
    if(http_parse_url(url,host_addr,file,&port)){  
        printf("http_parse_url failed!\n");  
        return NULL;  
    }  
    //printf("host_addr : %s\tfile:%s\t,%d\n",host_addr,file,port);  
  
    socket_fd =  http_tcpclient_create(host_addr,port);  
    if(socket_fd < 0){  
        printf("http_tcpclient_create failed\n");  
        return NULL;  
    }  
  
    sprintf(lpbuf,HTTP_GET,file,host_addr,port);  
  
    if(http_tcpclient_send(socket_fd,lpbuf,strlen(lpbuf)) < 0){  
        printf("http_tcpclient_send failed..\n");  
        return NULL;  
    }
    //printf("发送请求:\n%s\n",lpbuf);
  
    if(http_tcpclient_recv(socket_fd,lpbuf) <= 0){  
        printf("http_tcpclient_recv failed\n");  
        return NULL;  
    }  
    http_tcpclient_close(socket_fd);  
  
    return http_parse_result(lpbuf);  
}

char * sendData(gchar *address, uint8_t values)
{
    char get_url[200] = {'\0'};
    sprintf(get_url, MY_HTTP_DEFAULT_URL, serverData, address, values);
    printf("准备请求url:%s\n", get_url);
    char * result = http_get(get_url);
    return result;
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

        } else if(ptr->d_type == 8) {   ///file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
            if(i==0){
            	strcpy(filesList, ptr->d_name);
            }else{
            	strcat(filesList, "#");
            	strcat(filesList, ptr->d_name);
            }

        } else if(ptr->d_type == 10) {  ///link file
            printf("d_name:%s/%s\n",basePath,ptr->d_name);
            if(i==0){
            	strcpy(filesList, ptr->d_name);
            }else{
            	strcat(filesList, "#");
            	strcat(filesList, ptr->d_name);
            }

        } else if(ptr->d_type == 4) {   ///dir
        	//递归所有子文件夹里面的文件
            // memset(base,'\0',sizeof(base));
            // strcpy(base,basePath);
            // strcat(base,"/");
            // strcat(base,ptr->d_name);
            // readFileList(base);
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
//end by willen

static gchar *opt_src = NULL;
static gchar *opt_dst = NULL;
static gchar *opt_dst_type = NULL;
static gchar *opt_value = NULL;
static gchar *opt_sec_level = NULL;
static bt_uuid_t *opt_uuid = NULL;
static int opt_start = 0x0001;
static int opt_end = 0xffff;
static int opt_handle = -1;
static int opt_mtu = 0;
static int opt_psm = 0;
static int opt_offset = 0;
static gboolean opt_primary = FALSE;
static gboolean opt_characteristics = FALSE;
static gboolean opt_char_read = FALSE;
static gboolean opt_listen = FALSE;
static gboolean opt_char_desc = FALSE;
static gboolean opt_char_write = FALSE;
static gboolean opt_char_write_req = FALSE;
static gboolean opt_interactive = FALSE;
static GMainLoop *event_loop;
static gboolean got_error = FALSE;
static GSourceFunc operation;

struct characteristic_data {
	GAttrib *attrib;
	uint16_t start;
	uint16_t end;
};

static void events_handler(const uint8_t *pdu, uint16_t len, gpointer user_data)
{
	GAttrib *attrib = user_data;
	uint8_t opdu[ATT_MAX_MTU];
	uint16_t handle, i, olen = 0;

	handle = att_get_u16(&pdu[1]);

	switch (pdu[0]) {
	case ATT_OP_HANDLE_NOTIFY:
		g_print("Notification handle = 0x%04x value: ", handle);
		break;
	case ATT_OP_HANDLE_IND:
		g_print("Indication   handle = 0x%04x value: ", handle);
		break;
	default:
		g_print("Invalid opcode\n");
		return;
	}

	//add by willen
	uint8_t mydata = 0;

	for (i = 3; i < len; i++) {
		g_print("%02x", pdu[i]);

		//add by willen
		mydata = pdu[i];
	}

	g_print("\n");

	//add by willen
	//标识蓝牙设备已经连接成功
	char shell[50] = "touch /tmp/willen/connected/";
	strcat(shell, opt_dst);
	printf("%s:%d\n", shell, system(shell));

	if(serverData == NULL){
		printf("尚未配置服务器接口地址！\n");
	} else {
		printf("返回结果:%s\n", sendData(opt_dst, mydata));
	}


	if (pdu[0] == ATT_OP_HANDLE_NOTIFY)
		return;

	olen = enc_confirmation(opdu, sizeof(opdu));

	if (olen > 0)
		g_attrib_send(attrib, 0, opdu[0], opdu, olen, NULL, NULL, NULL);
}

static gboolean listen_start(gpointer user_data)
{
	GAttrib *attrib = user_data;

	g_attrib_register(attrib, ATT_OP_HANDLE_NOTIFY, events_handler,
							attrib, NULL);
	g_attrib_register(attrib, ATT_OP_HANDLE_IND, events_handler,
							attrib, NULL);

	return FALSE;
}

static void connect_cb(GIOChannel *io, GError *err, gpointer user_data)
{
	GAttrib *attrib;

	if (err) {
		g_printerr("%s\n", err->message);
		got_error = TRUE;
		g_main_loop_quit(event_loop);
	}

	attrib = g_attrib_new(io);

	if (opt_listen)
		g_idle_add(listen_start, attrib);

	operation(attrib);
}

static void primary_all_cb(GSList *services, guint8 status, gpointer user_data)
{
	GSList *l;

	if (status) {
		g_printerr("Discover all primary services failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	for (l = services; l; l = l->next) {
		struct gatt_primary *prim = l->data;
		g_print("attr handle = 0x%04x, end grp handle = 0x%04x "
			"uuid: %s\n", prim->range.start, prim->range.end, prim->uuid);
	}

done:
	g_main_loop_quit(event_loop);
}

static void primary_by_uuid_cb(GSList *ranges, guint8 status,
							gpointer user_data)
{
	GSList *l;

	if (status != 0) {
		g_printerr("Discover primary services by UUID failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	for (l = ranges; l; l = l->next) {
		struct att_range *range = l->data;
		g_print("Starting handle: %04x Ending handle: %04x\n",
						range->start, range->end);
	}

done:
	g_main_loop_quit(event_loop);
}

static gboolean primary(gpointer user_data)
{
	GAttrib *attrib = user_data;

	if (opt_uuid)
		gatt_discover_primary(attrib, opt_uuid, primary_by_uuid_cb,
									NULL);
	else
		gatt_discover_primary(attrib, NULL, primary_all_cb, NULL);

	return FALSE;
}

static void char_discovered_cb(GSList *characteristics, guint8 status,
							gpointer user_data)
{
	GSList *l;

	if (status) {
		g_printerr("Discover all characteristics failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	for (l = characteristics; l; l = l->next) {
		struct gatt_char *chars = l->data;

		g_print("handle = 0x%04x, char properties = 0x%02x, char value "
			"handle = 0x%04x, uuid = %s\n", chars->handle,
			chars->properties, chars->value_handle, chars->uuid);
	}

done:
	g_main_loop_quit(event_loop);
}

static gboolean characteristics(gpointer user_data)
{
	GAttrib *attrib = user_data;

	gatt_discover_char(attrib, opt_start, opt_end, opt_uuid,
						char_discovered_cb, NULL);

	return FALSE;
}

static void char_read_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	uint8_t value[ATT_MAX_MTU];
	int i, vlen;

	if (status != 0) {
		g_printerr("Characteristic value/descriptor read failed: %s\n",
							att_ecode2str(status));
		goto done;
	}
	if (!dec_read_resp(pdu, plen, value, &vlen)) {
		g_printerr("Protocol error\n");
		goto done;
	}
	g_print("Characteristic value/descriptor: ");
	for (i = 0; i < vlen; i++)
		g_print("%02x ", value[i]);
	g_print("\n");

done:
	if (opt_listen == FALSE)
		g_main_loop_quit(event_loop);
}

static void char_read_by_uuid_cb(guint8 status, const guint8 *pdu,
					guint16 plen, gpointer user_data)
{
	struct characteristic_data *char_data = user_data;
	struct att_data_list *list;
	int i;

	if (status == ATT_ECODE_ATTR_NOT_FOUND &&
					char_data->start != opt_start)
		goto done;

	if (status != 0) {
		g_printerr("Read characteristics by UUID failed: %s\n",
							att_ecode2str(status));
		goto done;
	}

	list = dec_read_by_type_resp(pdu, plen);
	if (list == NULL)
		goto done;

	for (i = 0; i < list->num; i++) {
		uint8_t *value = list->data[i];
		int j;

		char_data->start = att_get_u16(value) + 1;

		g_print("handle: 0x%04x \t value: ", att_get_u16(value));
		value += 2;
		for (j = 0; j < list->len - 2; j++, value++)
			g_print("%02x ", *value);
		g_print("\n");
	}

	att_data_list_free(list);

done:
	g_free(char_data);
	g_main_loop_quit(event_loop);
}

static gboolean characteristics_read(gpointer user_data)
{
	GAttrib *attrib = user_data;

	if (opt_uuid != NULL) {
		struct characteristic_data *char_data;

		char_data = g_new(struct characteristic_data, 1);
		char_data->attrib = attrib;
		char_data->start = opt_start;
		char_data->end = opt_end;

		gatt_read_char_by_uuid(attrib, opt_start, opt_end, opt_uuid,
						char_read_by_uuid_cb, char_data);

		return FALSE;
	}

	if (opt_handle <= 0) {
		g_printerr("A valid handle is required\n");
		g_main_loop_quit(event_loop);
		return FALSE;
	}

	gatt_read_char(attrib, opt_handle, opt_offset, char_read_cb, attrib);

	return FALSE;
}

static void mainloop_quit(gpointer user_data)
{
	uint8_t *value = user_data;

	g_free(value);
	g_main_loop_quit(event_loop);
}

static gboolean characteristics_write(gpointer user_data)
{
	GAttrib *attrib = user_data;
	uint8_t *value;
	size_t len;

	if (opt_handle <= 0) {
		g_printerr("A valid handle is required\n");
		goto error;
	}

	if (opt_value == NULL || opt_value[0] == '\0') {
		g_printerr("A value is required\n");
		goto error;
	}

	len = gatt_attr_data_from_string(opt_value, &value);
	if (len == 0) {
		g_printerr("Invalid value\n");
		goto error;
	}

	gatt_write_cmd(attrib, opt_handle, value, len, mainloop_quit, value);

	return FALSE;

error:
	g_main_loop_quit(event_loop);
	return FALSE;
}

static void char_write_req_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	if (status != 0) {
		g_printerr("Characteristic Write Request failed: "
						"%s\n", att_ecode2str(status));
		goto done;
	}

	if (!dec_write_resp(pdu, plen)) {
		g_printerr("Protocol error\n");
		goto done;
	}

	g_print("Characteristic value was written successfully\n");

done:
	if (opt_listen == FALSE)
		g_main_loop_quit(event_loop);
}

static gboolean characteristics_write_req(gpointer user_data)
{
	GAttrib *attrib = user_data;
	uint8_t *value;
	size_t len;

	if (opt_handle <= 0) {
		g_printerr("A valid handle is required\n");
		goto error;
	}

	if (opt_value == NULL || opt_value[0] == '\0') {
		g_printerr("A value is required\n");
		goto error;
	}

	len = gatt_attr_data_from_string(opt_value, &value);
	if (len == 0) {
		g_printerr("Invalid value\n");
		goto error;
	}

	gatt_write_char(attrib, opt_handle, value, len, char_write_req_cb,
									NULL);

	return FALSE;

error:
	g_main_loop_quit(event_loop);
	return FALSE;
}

static void char_desc_cb(guint8 status, const guint8 *pdu, guint16 plen,
							gpointer user_data)
{
	struct att_data_list *list;
	guint8 format;
	int i;

	if (status != 0) {
		g_printerr("Discover all characteristic descriptors failed: "
						"%s\n", att_ecode2str(status));
		goto done;
	}

	list = dec_find_info_resp(pdu, plen, &format);
	if (list == NULL)
		goto done;

	for (i = 0; i < list->num; i++) {
		char uuidstr[MAX_LEN_UUID_STR];
		uint16_t handle;
		uint8_t *value;
		bt_uuid_t uuid;

		value = list->data[i];
		handle = att_get_u16(value);

		if (format == 0x01)
			uuid = att_get_uuid16(&value[2]);
		else
			uuid = att_get_uuid128(&value[2]);

		bt_uuid_to_string(&uuid, uuidstr, MAX_LEN_UUID_STR);
		g_print("handle = 0x%04x, uuid = %s\n", handle, uuidstr);
	}

	att_data_list_free(list);

done:
	if (opt_listen == FALSE)
		g_main_loop_quit(event_loop);
}

static gboolean characteristics_desc(gpointer user_data)
{
	GAttrib *attrib = user_data;

	gatt_find_info(attrib, opt_start, opt_end, char_desc_cb, NULL);

	return FALSE;
}

static gboolean parse_uuid(const char *key, const char *value,
				gpointer user_data, GError **error)
{
	if (!value)
		return FALSE;

	opt_uuid = g_try_malloc(sizeof(bt_uuid_t));
	if (opt_uuid == NULL)
		return FALSE;

	if (bt_string_to_uuid(opt_uuid, value) < 0)
		return FALSE;

	return TRUE;
}

static GOptionEntry primary_char_options[] = {
	{ "start", 's' , 0, G_OPTION_ARG_INT, &opt_start,
		"Starting handle(optional)", "0x0001" },
	{ "end", 'e' , 0, G_OPTION_ARG_INT, &opt_end,
		"Ending handle(optional)", "0xffff" },
	{ "uuid", 'u', G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK,
		parse_uuid, "UUID16 or UUID128(optional)", "0x1801"},
	{ NULL },
};

static GOptionEntry char_rw_options[] = {
	{ "handle", 'a' , 0, G_OPTION_ARG_INT, &opt_handle,
		"Read/Write characteristic by handle(required)", "0x0001" },
	{ "value", 'n' , 0, G_OPTION_ARG_STRING, &opt_value,
		"Write characteristic value (required for write operation)",
		"0x0001" },
	{ "offset", 'o', 0, G_OPTION_ARG_INT, &opt_offset,
		"Offset to long read characteristic by handle", "N"},
	{NULL},
};

static GOptionEntry gatt_options[] = {
	{ "primary", 0, 0, G_OPTION_ARG_NONE, &opt_primary,
		"Primary Service Discovery", NULL },
	{ "characteristics", 0, 0, G_OPTION_ARG_NONE, &opt_characteristics,
		"Characteristics Discovery", NULL },
	{ "char-read", 0, 0, G_OPTION_ARG_NONE, &opt_char_read,
		"Characteristics Value/Descriptor Read", NULL },
	{ "char-write", 0, 0, G_OPTION_ARG_NONE, &opt_char_write,
		"Characteristics Value Write Without Response (Write Command)",
		NULL },
	{ "char-write-req", 0, 0, G_OPTION_ARG_NONE, &opt_char_write_req,
		"Characteristics Value Write (Write Request)", NULL },
	{ "char-desc", 0, 0, G_OPTION_ARG_NONE, &opt_char_desc,
		"Characteristics Descriptor Discovery", NULL },
	{ "listen", 0, 0, G_OPTION_ARG_NONE, &opt_listen,
		"Listen for notifications and indications", NULL },
	{ "interactive", 'I', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,
		&opt_interactive, "Use interactive mode", NULL },
	{ NULL },
};

static GOptionEntry options[] = {
	{ "adapter", 'i', 0, G_OPTION_ARG_STRING, &opt_src,
		"Specify local adapter interface", "hciX" },
	{ "device", 'b', 0, G_OPTION_ARG_STRING, &opt_dst,
		"Specify remote Bluetooth address", "MAC" },
	{ "addr-type", 't', 0, G_OPTION_ARG_STRING, &opt_dst_type,
		"Set LE address type. Default: public", "[public | random]"},
	{ "mtu", 'm', 0, G_OPTION_ARG_INT, &opt_mtu,
		"Specify the MTU size", "MTU" },
	{ "psm", 'p', 0, G_OPTION_ARG_INT, &opt_psm,
		"Specify the PSM for GATT/ATT over BR/EDR", "PSM" },
	{ "sec-level", 'l', 0, G_OPTION_ARG_STRING, &opt_sec_level,
		"Set security level. Default: low", "[low | medium | high]"},
	{ NULL },
};

int main(int argc, char *argv[])
{

	//add by willen
	//保存服务器接口地址的信息
	serverData = malloc(20);
	memset(serverData, 0, 20);
	//读取服务器的接口地址信息
	if( readFileList("/tmp/willen/server", serverData) != 0 ) {
		//读取信息失败
		serverData = NULL;
	}

	GOptionContext *context;
	GOptionGroup *gatt_group, *params_group, *char_rw_group;
	GError *gerr = NULL;
	GIOChannel *chan;

	opt_dst_type = g_strdup("public");
	opt_sec_level = g_strdup("low");

	context = g_option_context_new(NULL);
	g_option_context_add_main_entries(context, options, NULL);

	/* GATT commands */
	gatt_group = g_option_group_new("gatt", "GATT commands",
					"Show all GATT commands", NULL, NULL);
	g_option_context_add_group(context, gatt_group);
	g_option_group_add_entries(gatt_group, gatt_options);

	/* Primary Services and Characteristics arguments */
	params_group = g_option_group_new("params",
			"Primary Services/Characteristics arguments",
			"Show all Primary Services/Characteristics arguments",
			NULL, NULL);
	g_option_context_add_group(context, params_group);
	g_option_group_add_entries(params_group, primary_char_options);

	/* Characteristics value/descriptor read/write arguments */
	char_rw_group = g_option_group_new("char-read-write",
		"Characteristics Value/Descriptor Read/Write arguments",
		"Show all Characteristics Value/Descriptor Read/Write "
		"arguments",
		NULL, NULL);
	g_option_context_add_group(context, char_rw_group);
	g_option_group_add_entries(char_rw_group, char_rw_options);

	if (g_option_context_parse(context, &argc, &argv, &gerr) == FALSE) {
		g_printerr("%s\n", gerr->message);
		g_error_free(gerr);
	}

	if (opt_interactive) {
		interactive(opt_src, opt_dst, opt_dst_type, opt_psm);
		goto done;
	}

	if (opt_primary)
		operation = primary;
	else if (opt_characteristics)
		operation = characteristics;
	else if (opt_char_read)
		operation = characteristics_read;
	else if (opt_char_write)
		operation = characteristics_write;
	else if (opt_char_write_req)
		operation = characteristics_write_req;
	else if (opt_char_desc)
		operation = characteristics_desc;
	else {
		gchar *help = g_option_context_get_help(context, TRUE, NULL);
		g_print("%s\n", help);
		g_free(help);
		got_error = TRUE;
		goto done;
	}

	chan = gatt_connect(opt_src, opt_dst, opt_dst_type, opt_sec_level,
					opt_psm, opt_mtu, connect_cb);
	if (chan == NULL) {
		got_error = TRUE;
		goto done;
	}

	event_loop = g_main_loop_new(NULL, FALSE);

	g_main_loop_run(event_loop);

	g_main_loop_unref(event_loop);

done:
	g_option_context_free(context);
	g_free(opt_src);
	g_free(opt_dst);
	g_free(opt_uuid);
	g_free(opt_sec_level);

	if (got_error)
		exit(EXIT_FAILURE);
	else
		exit(EXIT_SUCCESS);
}
