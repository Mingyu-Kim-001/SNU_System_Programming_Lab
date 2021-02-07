#define UNW_LOCAL_ONLY

#include <stdlib.h>
#include <libunwind.h>
#include <string.h>

int get_callinfo(char *fname, size_t fnlen, unsigned long long *ofs)
{
  unw_context_t context;
  unw_cursor_t cursor;
  unw_word_t* off;
  unw_proc_info_t pip;
  char procname[256];
  int ret;
  if(unw_getcontext(&context)){
    return -1;
  }
  if(unw_init_local(&cursor, &context)){
    return -1;
  }
  while(unw_step(&cursor)>0){
    if(unw_get_proc_name(&cursor, fname, fnlen, (unw_word_t*)ofs)){
      return -1;
    }
    if(!strcmp(fname,"main")){
      break;
    }
  }
  if(unw_get_proc_name(&cursor, fname, fnlen, (unw_word_t*)ofs)){
    return -1;
  }
  //*ofs = (*off);
  *ofs = (*ofs) - 5;
  return 0;
}
