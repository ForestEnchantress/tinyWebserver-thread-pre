#ifndef TINY_H_
#define TINY_H_ 
#include"csapp.h"
void doit(int fd);
int read_requesthdrs(rio_t* rp,char* post_content,int rmtd);
int parse_uri(char* uri,char* filename,char* cgiargs);
void serve_static(int fd,char* filename,int filesize,int rmtd);
void get_filetype(char* filename,char* filetype);
void serve_dynamic(int fd,char* filename,char* cgiargs,int rmtd);
void clienterror(int fd,char* cause,char* errnum,char* shortmsg,char* longmsg);
#include"tiny.c"
#endif
