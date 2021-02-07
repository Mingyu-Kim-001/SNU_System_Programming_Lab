#include <stdio.h>
#include "csapp.h"
#include "cache.h"
/* Recommended max cache and object sizes */

#define MAX_PORT_NUM 65536

#define USE_CACHE 1
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *proxy_connection_hdr = "Proxy-Connection: close\r\n";

typedef struct{
	char method[MAXLINE];
	char uri[MAXLINE];
	char version[MAXLINE];
	char hostname[MAXLINE];
	char pathname[MAXLINE];
	char port[10];
} Request;


void *handle_client(void *vargp);
void parse_request(char request[MAXLINE], Request *req);
void parse_uri(Request *req);
void parse_header(rio_t *riop,char header[MAXLINE], Request *req);
int get_from_cache(Request *req,int connfd);


int main(int argc, char **argv){
        int port, listenfd, *connfdp;
        pthread_t tid;
        struct sockaddr_in clientaddr;
        int clientlen = sizeof(clientaddr);
        if(argc!=2){
                fprintf(stderr,"usage : ./proxy <port_num>\n");
                exit(0);
        }
        Signal(SIGPIPE,SIG_IGN);
        port = atoi(argv[1]);
        if(port<0 || port>MAX_PORT_NUM){
                printf("invalid port number\n");
                exit(1);
        }
	cache_init();
        listenfd = Open_listenfd(argv[1]);
        while(1){
                connfdp = -1;
                connfdp = Malloc(sizeof(int));
                *connfdp = -1;
                *connfdp = Accept(listenfd, (SA *) &clientaddr, (socklen_t *)&clientlen);
                Pthread_create(&tid,NULL,handle_client,(void *)connfdp);
        }
        return 0;
}

void *handle_client(void *vargp){
	char buf[MAXLINE];
	char header[MAXLINE];
	int connfd = *((int *)vargp);

	Request *req = Malloc(sizeof(Request));
	Pthread_detach(Pthread_self());
	Free(vargp);
	
	rio_t rio;
	Rio_readinitb(&rio,connfd);
	Rio_readlineb(&rio,buf,MAXLINE);
	parse_request(buf,req);
	if(strcasecmp(req->method,"GET")){
		char errormsg[MAXLINE];
		sprintf(errormsg,"%s\n","only method GET allowed");
		Rio_writen(connfd,errormsg,strlen(errormsg));
		return;
	}
	parse_uri(req);
	if(!get_from_cache(req,connfd)){
		get_from_server(&rio,req,connfd);
	}

	Close(connfd);
	Free(req);
	return;
}

void parse_request(char request[MAXLINE], Request *req){
	sscanf(request,"%s %s %s",req->method,req->uri,req->version);
	return;
}
void parse_uri(Request *req){
	if(strncasecmp(req->uri,"http://",7)){ //if uri does not start with "http://"
		(req->hostname)[0] = '\0';
		return -1;
	}
	char *hostname_start, *hostname_end;
	int port;
	hostname_start = (req->uri) + 7;
	hostname_end = strpbrk(hostname_start,":/\r\n\0");
	strncpy(req->hostname,hostname_start,(int)(hostname_end - hostname_start));
	(req->hostname)[(int)(hostname_end - hostname_start)] = '\0'; //end of the string
	if(*hostname_end == ':'){
		port = atoi(hostname_end+1);
	}
	else{
		port = 80; // default port is 80
	}
	sprintf(req->port,"%d",port);
	char *pathname_start = strchr(hostname_start,'/');
	if(pathname_start==NULL){
		(req->pathname)[0] = '\0';
	}
	else{
		strcpy((req->pathname),++pathname_start);
	}
	return 0;
}

void parse_header(rio_t *riop,char header[MAXLINE], Request *req){
	char buf[MAXLINE];
	int isHost=0;
	sprintf(header,"%s%s%s%s%s",user_agent_hdr,accept_hdr,accept_encoding_hdr,connection_hdr,proxy_connection_hdr);
	Rio_readlineb(riop, buf, MAXLINE);
	while(strcmp(buf,"\r\n")){
		if(!strstr(buf,"User-Agent") && !strstr(buf,"Accept") && !strstr(buf,"Connection") && !strstr(buf,"Proxy-Connection")){
			if(strstr(buf,"Host")){
				isHost = 1;
			}
			strcat(header,buf);
		}
		Rio_readlineb(riop, buf, MAXLINE);
	}
	if(!isHost){
		sprintf(header, "%sHost: %s\r\n", header, req->hostname);
	}
	strcat(header, "\r\n");
	return;
}

int get_from_cache(Request *req,int connfd){
	CachedItem *c;
	int found;
	P(&mutex);
	readcnt++;
	if(readcnt==1){
		P(&w);
	}
	V(&mutex);
	if((c=find_cache(req->hostname,atoi(req->port),req->pathname))!=NULL){
		printf("cache hit\n");
		Rio_writen(connfd,c->data,c->size);
		found = 1;
	}
	else{
		found = 0;	
	}
	P(&mutex);
	readcnt--;
	if(readcnt == 0){
		V(&w);
	}
	V(&mutex);
	return found;
}

void get_from_server(rio_t *riop,Request *req, int connfd){
	int clientfd;
	char buf[MAXLINE];
	char header[MAXLINE];
	char data[MAX_OBJECT_SIZE];
	int readcnt2;
	parse_header(riop,header,req);
	if((clientfd = Open_clientfd(req->hostname,req->port))<0){
		return;
	}
	sprintf(buf, "%s /%s HTTP/1.0\r\n",req->method,req->pathname);
	Rio_writen(clientfd,buf,strlen(buf));
	Rio_writen(clientfd,header,strlen(header));
	int cnt = 0;
	while((readcnt2 = Rio_readn(clientfd,buf,MAXLINE))>0){
		if(cnt + readcnt<=MAX_OBJECT_SIZE){
			memcpy(data+cnt,buf,readcnt2);
			cnt+= readcnt2;
		}
		Rio_writen(connfd,buf,readcnt2);
	}
	if(cnt>0) printf("add size %d object to cache\n",cnt);
	P(&w);
	CachedItem *c = create_CI(req->hostname,req->port, req->pathname, data);
	insert_cache(c);
	V(&w);
	Close(clientfd);

}
