#include"tiny.h"
#include"csapp.h"
#define M_GET 0
#define M_POST 1
#define M_HEAD 2
#define M_NONE (-1)

void doit(int fd){
	int is_static,rmtd=0;
	struct stat sbuf;
	char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];
	char filename[MAXLINE],cgiargs[MAXLINE];
	rio_t rio;
	
	/*for post*/
	int contentLen;
	char post_content[MAXLINE];
	
	Rio_readinitb(&rio,fd);
	if(!Rio_readlineb(&rio,buf,MAXLINE))
		return;
	printf("Request headers:\n");
	printf("%s",buf);
	sscanf(buf,"%s %s %s",method,uri,version);
	if(strcasecmp(method,"GET")==0) rmtd=M_GET;
	else if(strcasecmp(method,"POST")==0) rmtd=M_POST;
	else if(strcasecmp(method,"HEAD")==0) rmtd=M_HEAD;
	else rmtd=M_NONE;
	if(rmtd==M_NONE){/*judge if method exists*/
		clienterror(fd,method,"501","Not implemented","Tiny does not implement this method");
		return;
	}
	contentLen=read_requesthdrs(&rio,post_content,rmtd);
	/*parse URI from request*/
	is_static=parse_uri(uri,filename,cgiargs);
	
		
	if(stat(filename,&sbuf)<0){
		clienterror(fd,filename,"404","Not found","Tiny counldn't read the file");
		return;
	}
	if(is_static){/*Serve static content*/
		if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR&sbuf.st_mode)){
			clienterror(fd,filename,"403","Forbidden","Tiny couldn't read the file");
			return;
		}
		serve_static(fd,filename,sbuf.st_size,rmtd);
	}
	else{/*serve dynamic content*/
		if(!(S_ISREG(sbuf.st_mode))||!(S_IXUSR&sbuf.st_mode)){
			clienterror(fd,filename,"403","Forbidden","Tiny couldn't run the CGI program");
			return;
		}
		if(rmtd==M_POST) strcpy(cgiargs,post_content);
		serve_dynamic(fd,filename,cgiargs,rmtd);
	}

}
void clienterror(int fd,char* cause,char* errnum,char* shortmsg,char* longmsg){
	char buf[MAXLINE],body[MAXBUF];

	/*Build the HTTP response body*/
	sprintf(body,"<html><title>Tiny Error</title>");
	sprintf(body,"%s<body bgcolor=""ffffff"">\r\n",body);
	sprintf(body,"%s%s: %s\r\n",body,errnum,shortmsg);
	sprintf(body,"%s<p>%s: %s\r\n",body,longmsg,cause);
	sprintf(body,"%s<hr><em>The Tiny Web server</em>\r\n",body);

	/*Print the HTTP response*/
	sprintf(buf,"HTTP/1.0 %s %s\r\n",errnum,shortmsg);
	Rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-Type:text/html\r\n");
	Rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Content-Length:%d\r\n\r\n",(int)strlen(body));
	Rio_writen(fd,buf,strlen(buf));
	Rio_writen(fd,body,strlen(body));
}
int read_requesthdrs(rio_t* rp,char* post_content,int rmtd){
	char buf[MAXLINE];
	int contentLength=0;
	Rio_readlineb(rp,buf,MAXLINE);
	printf("%s",buf);
	while(strcmp(buf,"\r\n")){
		Rio_readlineb(rp,buf,MAXLINE);
		printf("%s",buf);
		if(rmtd==M_POST&&strstr(buf,"Content-length:")==buf)
			contentLength=atoi(buf+strlen("Content-length:"));
	}
	if(rmtd==M_POST){
		contentLength=rio_readnb(rp,post_content,contentLength);
		post_content[contentLength]=0;
		printf("POST_CONTENT:%s\n",post_content);
	}
	return contentLength;
}

int parse_uri(char* uri,char* filename,char* cgiargs){/*Static content*/
	char* ptr;

	if(!strstr(uri,"cgi-bin")){
		strcpy(cgiargs,"");
		strcpy(filename,".");
		strcat(filename,uri);
		if(uri[strlen(uri)-1]=='/')
			strcat(filename,"home.html");
		return 1;
	}
	else{		/*Dynamic content*/
		ptr=index(uri,'?');
		if(ptr){
			strcpy(cgiargs,ptr+1);
			*ptr='\0';
		}
		else 
			strcpy(cgiargs,"");
		strcpy(filename,".");
		strcat(filename,uri);
		return 0;
	}
}

void serve_static(int fd,char* filename,int filesize,int rmtd){
	int srcfd;
	char* srcp,filetype[MAXLINE],buf[MAXBUF];

	/*send response headers to client*/
	get_filetype(filename,filetype);
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	sprintf(buf,"%sServer:Tiny Web Server\r\n",buf);
	sprintf(buf,"%sConnection:close\r\n",buf);
	sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
	sprintf(buf,"%sContent-type:%s\r\n\r\n",buf,filetype);
	Rio_writen(fd,buf,strlen(buf));
	
	/*Send response body to client*/
	if(rmtd!=M_HEAD){
		srcfd=Open(filename,O_RDONLY,0);
		srcp=Mmap(0,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);		//create virtual memory
		Close(srcfd);
		Rio_writen(fd,srcp,filesize);
		
		Munmap(srcp,filesize);	//remove memory mapping}	
	}
}
void get_filetype(char* filename,char* filetype){
	if(strstr(filename,".html"))
		strcpy(filetype,"text/html");
	else if(strstr(filename,".gif"))
		strcpy(filetype,"image/gif");
	else if(strstr(filename,".png"))
		strcpy(filetype,"image/png");
	else if(strstr(filename,".jpg"))
		strcpy(filetype,"image/jpeg");
	else if(strstr(filename,".mpg"))
		strcpy(filetype,"video/mpeg");
	else 
		strcpy(filetype,"text/plain");
}
void serve_dynamic(int fd,char* filename,char* cgiargs,int rmtd){
	char buf[MAXLINE],* emptylist[]={NULL};

	/*Return first part of HTTP response*/
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	Rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Server:Tiny Web Server\r\n");
	Rio_writen(fd,buf,strlen(buf));


	if(rmtd!=M_HEAD){
		if(Fork()==0){/*Child*/
			/*Real server would set all CGI vars here*/
			setenv("QUERY_STRING",cgiargs,1);
			Dup2(fd,STDOUT_FILENO);	/*Redirect stdout to client*/
			Execve(filename,emptylist,environ);
		}
	}
	Wait(NULL);	/*Parents wait for and reaps child*/
}
