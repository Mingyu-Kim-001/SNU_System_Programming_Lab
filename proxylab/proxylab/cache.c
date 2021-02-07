#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

int total_size, num_CachedItem;
CachedItem *tail, *head;
sem_t mutex,w;
volatile int readcnt; 
void free_CachedItem(CachedItem* c){
	Free(c->hostname);
	Free(c->pathname);
	Free(c->data);
	Free(c);
}

void cache_init(){
	tail = NULL;
	head = NULL;
	total_size = 0;
	num_CachedItem = 0;
	readcnt = 0;
	Sem_init(&mutex,0,1);
	Sem_init(&w,0,1);
}

void remove_LRU(){
	CachedItem *LRU;
	if(num_CachedItem == 0){
		return;
	}
	else if(num_CachedItem == 1){
		LRU = head;
		head = NULL;
		tail = NULL;
	}
	else{
		LRU = head;
		head = LRU->next;
		head->prev = NULL;
	}
	total_size-= LRU->size;
	num_CachedItem--;
	free_CachedItem(LRU);
}

CachedItem *create_CI(char *hostname,char *port, char *pathname, char *data){
	CachedItem *c = Malloc(sizeof(CachedItem));
	if(hostname!=NULL){
		c->hostname = Malloc(strlen(hostname)+1);
		strcpy(c->hostname,hostname);
	}

	if(pathname!=NULL){
		c->pathname = Malloc(strlen(pathname)+1);
		strcpy(c->pathname,pathname);
	}
	if(port!=NULL){
		c->port = atoi(port);
	}

	if(data!=NULL){
		c->data = Malloc(strlen(data)+1);
		strcpy(c->data,data);
		c->size = strlen(data)+1;
	}

	return c;
}
void insert_cache(CachedItem* c){
	if(c->size > MAX_OBJECT_SIZE){//cache object of which size is less than MAX_OBJECT_SIZE
		return;
	}
	while((total_size + c->size) > MAX_CACHE_SIZE){
		remove_LRU();
	}
	if(num_CachedItem == 0){//if there was no cached item
		head = c;
		tail = c;
		c->next = NULL;
		c->prev = NULL;
	}
	else{//insert to the end
		c->prev = tail;
		tail->next = c;
		tail = c;
		c->next = NULL;
	}
	total_size+= c->size; // correct lately
	num_CachedItem++;
	//printCachedItems();

}


int isSame(CachedItem *c,char *hostname, int port, char *pathname){
	if(strcasecmp(c->hostname,hostname) || strcasecmp(c->pathname,pathname)){
		return 0;
	}
	return 1;
}
CachedItem *find_cache(char *hostname, int port, char *pathname){
	for(CachedItem *curr = tail; curr!=NULL; curr=curr->prev){
		if(isSame(curr,hostname,port,pathname)){
			move_to_end(curr); // to keep LRU
			return curr;
		}
	}
	return NULL;
}
void move_to_end(CachedItem *c){
	if(c->next==NULL){//if c is already on the end
		return;
	}
	if(c==head){// if c is head
		((CachedItem *)(c->next))->prev = NULL;
		c->prev = tail;
		tail->next = c;
		tail = c;
		c->next = NULL;
	}
	else{
		((CachedItem *)(c->next))->prev = c->prev;
		((CachedItem *)(c->prev))->next = c->next;
		c->prev = tail;
		tail->next = c;
		tail = c;
		c->next = NULL;
	}
	//printCachedItems();
}
void printCachedItems(){
	printf("cached_list\n");
	for(CachedItem *curr = tail; curr!=NULL; curr=(curr->prev)){
		printf("%s -> ",curr->pathname);
	}
	printf("\n");
}
