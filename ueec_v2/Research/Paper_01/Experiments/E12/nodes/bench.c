#define main original_main
#include "original.c"
#undef main
#include <time.h>
static uint64_t ns(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC_RAW,&t))abort();return (uint64_t)t.tv_sec*1000000000ULL+t.tv_nsec;}
int main(int argc,char**argv){if(argc!=5)return 2;int fd=open(argv[1],O_RDONLY);struct stat st;if(fd<0||fstat(fd,&st))return 2;file_view f={.data=mmap(NULL,st.st_size,PROT_READ,MAP_PRIVATE,fd,0),.size=(size_t)st.st_size};if(f.data==MAP_FAILED)return 2;ueec_view v;size_t selected;int n=atoi(argv[3]),reps=atoi(argv[4]);for(int i=-10;i<reps;i++){uint64_t a=ns();if(parse_ueec(&f,&v))return 3;uint64_t b=ns();if(select_node(&f,&v,argv[2],&selected)||selected!=(size_t)n)return 4;uint64_t c=ns();if(i>=0)printf("%d,%"PRIu64",%"PRIu64"\n",i,b-a,c-b);}munmap((void*)f.data,f.size);close(fd);return 0;}
