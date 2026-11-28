#include "resolve_bench.inc"
#include <time.h>
static uint64_t now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC_RAW,&t))exit(2);return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
int main(int argc,char **argv){
 if(argc!=7)return 2;
 int n=atoi(argv[3]),d=atoi(argv[4]),reps=atoi(argv[5]);load_mode mode;if(parse_mode(argv[6],&mode))return 2;
 module app={.fd=-1};library lib={.name="lib",.m={.fd=-1}};
 if(load(&app,argv[1],mode)||load(&lib.m,argv[2],mode))return 3;
 cursor_t c;if(lib.m.im.export_count!=1||section_cursor(&lib.m,lib.m.im.export_global[0],&c))return 4;
 int indices[]={0,n/2,n-1,n};
 for(int k=0;k<4;k++){
  char sym[32];snprintf(sym,sizeof sym,"s%05d",indices[k]);
  for(int r=-5;r<reps;r++){
   query_t q={.name=sym,.ordinal=(uint64_t)indices[k]+1,.byord=d==2};
   uint64_t a=now();int rc=table(c,&lib.m.im,&q);uint64_t b=now();
   if(rc||q.found!=(k!=3)||(q.found&&(q.section!=2||q.offset!=(uint64_t)indices[k])))return 5;
   if(r>=0)fprintf(stderr,"lookup%d,%d,%llu\n",k,r,(unsigned long long)(b-a));
  }
 }
 for(int r=0;r<reps;r++){
  memset((void*)app.im.section_address[2],0,(size_t)n*8);
  uint64_t a=now();int rc=bind_imports(&app,&lib,1,0);uint64_t b=now();if(rc)return 6;
  for(int i=0;i<n;i++){uintptr_t p;memcpy(&p,(void*)(app.im.section_address[2]+8*i),8);if(p!=lib.m.im.section_address[2]+(size_t)i)return 7;}
  if(r>=0)fprintf(stderr,"bind,%d,%llu\n",r,(unsigned long long)(b-a));
 }
 unload(&app);unload(&lib.m);return 0;
}
