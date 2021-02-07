//------------------------------------------------------------------------------
//
// memtrace
//
// trace calls to the dynamic memory manager
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memlog.h>
#include <memlist.h>
#include "callinfo.h"

//
// function pointers to stdlib's memory management functions
//
static void *(*mallocp)(size_t size) = NULL;
static void (*freep)(void *ptr) = NULL;
static void *(*callocp)(size_t nmemb, size_t size);
static void *(*reallocp)(void *ptr, size_t size);

//
// statistics & other global variables
//
static unsigned long n_malloc  = 0;
static unsigned long n_calloc  = 0;
static unsigned long n_realloc = 0;
static unsigned long n_allocb  = 0;
static unsigned long n_freeb   = 0;
static item *list = NULL;

//
// init - this function is called once when the shared library is loaded
//
__attribute__((constructor))
void init(void)
{
  char *error;

  LOG_START();

  // initialize a new list to keep track of all memory (de-)allocations
  // (not needed for part 1)
  list = new_list();

  // ...
}
//
// fini - this function is called once when the shared library is unloaded
//
__attribute__((destructor))
void fini(void)
{
  // ...

  LOG_STATISTICS(n_allocb,n_allocb/(n_malloc+n_calloc+n_realloc), n_freeb);
  //LOG_NONFREED_START();
  item *i = list->next;
  //item *j = i;
  int isAnyBlock = 0;
  while(i!=NULL){
    if(i->cnt!=0){
      isAnyBlock = 1;
      break;
    }
    i = i->next;
  }
  i = list->next;
  if(isAnyBlock){
    LOG_NONFREED_START();
    while (i != NULL) {
      if(i->cnt!=0){
          LOG_BLOCK(i->ptr, i->size, i->cnt, i->fname, i->ofs);
      } 
      i = i->next;
    }

  } 
  

  
  LOG_STOP();

  // free list (not needed for part 1)
  free_list(list);
}

// ...
void *malloc(size_t size){
	char *error;
	void *ptr;
	if(!mallocp){
		mallocp = dlsym(RTLD_NEXT,"malloc");
		if((error = dlerror())!=NULL){
			fputs(error,stderr);
			exit(1);
		}
	}
	ptr = mallocp(size);
	n_malloc+= 1;
	n_allocb+= size;
	alloc(list,ptr,size);
	
	
	LOG_MALLOC(size,ptr);
	return ptr;
}
void *calloc(size_t nmemb, size_t size){
        char *error;
        void *ptr;
        if(!callocp){
                callocp = dlsym(RTLD_NEXT,"calloc");
                if((error = dlerror())!=NULL){
                        fputs(error,stderr);
                        exit(1);
                }
        }
        ptr = callocp(nmemb,size);
        n_calloc+= 1;
	n_allocb+= nmemb*size;
	alloc(list,ptr,nmemb*size);
	
        LOG_CALLOC(nmemb,size,ptr);
        return ptr;
}
void *realloc(void *ptr,size_t size){
        char *error;
        if(!reallocp){
                reallocp = dlsym(RTLD_NEXT,"realloc");
                if((error = dlerror())!=NULL){
                        fputs(error,stderr);
                        exit(1);
                }
        }
        void *res;
        item *i = find(list,ptr);
        if(i!=NULL&&i->cnt>0){//if free can be completed for ptr
                n_freeb+= i->size;
		res = reallocp(ptr,size);
		LOG_REALLOC(ptr,size,res);
		/*
		if(res!=ptr){//if other pointer rather than original one is allocated, update list(free and allocate)
                	alloc(list,res,size);
                	dealloc(list,ptr);
        	}
	        else{//if same space is allocated, update list size
        	        find(list,ptr)->size = size;
       		}*/
		dealloc(list,ptr);
		alloc(list,res,size);


        }
	//res = reallocp(ptr,size); by the bonus_realloc_ill
	//LOG_REALLOC(ptr,size,res);
	else if(i == NULL){// illegal free
		res = reallocp(NULL,size);
		LOG_REALLOC(ptr,size,res);
		LOG_ILL_FREE();
		alloc(list,res,size);
	}
	else if(i->cnt == 0){// double free
		//res = mallocp(size);
		res = reallocp(NULL,size);
		LOG_REALLOC(ptr,size,res);
		LOG_DOUBLE_FREE();
		alloc(list,res,size);
	}	
        
        //res = reallocp(ptr,size);
        n_realloc+= 1;
        n_allocb+= size;
        
        return res;
}


void free(void *ptr){
	char *error;
	if(!freep){
		freep = dlsym(RTLD_NEXT,"free");
                if((error = dlerror())!=NULL){
                        fputs(error,stderr);
                        exit(1);
                }

	}
	item* i = find(list,ptr);
	if(i == NULL){
		LOG_FREE(ptr);
		LOG_ILL_FREE();
		return;
	}
	if(i->cnt == 0){
		LOG_FREE(ptr);
		LOG_DOUBLE_FREE();
		return;
	}
	item* freed = dealloc(list,ptr);
	n_freeb+= freed->size;
	freep(ptr);
	LOG_FREE(ptr);
	return;
}
