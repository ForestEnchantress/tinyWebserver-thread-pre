#include"tiny.h"
#include"sbuf.h"
#include"csapp.h"

#define SBUFSIZE 4
#define INIT_THREAD_N 1
#define THREAD_LIMIT 4096
int mainpid;
sbuf_t sbuf;
static int nthreads;
typedef struct{
	pthread_t tid;
	sem_t mutex;
}ithread;
static ithread threads[THREAD_LIMIT];

void handler2(int sig){
	if(getpid()!=mainpid)
		exit(0);
}
void init();
void* serve_thread(void* vagrp);
void* adjust_threads(void* vargp);
void create_threads(int start,int end);

int main(int argc,char** argv){
	int listenfd,connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	pthread_t tid;

	if(argc!=2){
		fprintf(stderr,"usage:%s port\n",argv[0]);
		exit(0);
	}
	listenfd=Open_listenfd(atoi(argv[1]));
	init();

	Pthread_create(&tid,NULL,adjust_threads,NULL);

	while(1){
		clientlen=sizeof(struct sockaddr_storage);
		connfd=Accept(listenfd,(SA*)&clientaddr,&clientlen);
		sbuf_insert(&sbuf,connfd);
	}
}
void init(){
	nthreads=INIT_THREAD_N;
	sbuf_init(&sbuf,SBUFSIZE);
	create_threads(0,nthreads);
}
void* serve_thread(void* vargp){
	int idx=*((int*)vargp);
	Free(vargp);

	while(1){
		P(&(threads[idx].mutex));
		int connfd=sbuf_remove(&sbuf);
		doit(connfd);
		Close(connfd);
		V(&(threads[idx].mutex));
	}
}
void create_threads(int start,int end){
	int i;
	for(i=start;i<end;i++){
		Sem_init(&(threads[i].mutex),0,1);
		int* arg=(int*)Malloc(sizeof(int));
		*arg=i;
		Pthread_create(&(threads[i].tid),NULL,serve_thread,arg);
	}
}
void* adjust_threads(void* vargp){
	sbuf_t* sp=&sbuf;

	while(1){
		if(sbuf_full(sp)){
			if(nthreads==THREAD_LIMIT){
				fprintf(stderr,"too many threads,can't double\n");
				continue;
			}

			int dn=2*nthreads;
			create_threads(nthreads,dn);
			nthreads=dn;
			continue;
		}

		if(sbuf_empty(sp)){
			if(nthreads==1)
				continue;
			int hn=nthreads/2;
			int i;
			for(i=hn;i<nthreads;i++){
				P(&(threads[i].mutex));
				Pthread_cancel(threads[i].tid);
				V(&(threads[i].mutex));
			}
			nthreads=hn;
			continue;
		}
	}
}

