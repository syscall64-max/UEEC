#define main original_main
#include "load.c"
#undef main
#include <sys/resource.h>
static int check(uintptr_t addr,int op,int allowed,int write_ret){
 pid_t p=fork(); if(p<0)return -1;
 if(!p){signal(SIGSEGV,SIG_DFL);signal(SIGBUS,SIG_DFL);volatile uint8_t *q=(volatile uint8_t*)addr;
 if(op==0){volatile uint8_t v=*q;(void)v;}
 if(op==1)*q=*q;
 if(op==2){if(write_ret)*q=0xc3; ((void(*)(void))addr)();}
 _exit(0);}
 int st;if(waitpid(p,&st,0)!=p)return -1;
 return allowed ? !(WIFEXITED(st)&&WEXITSTATUS(st)==0) : !(WIFSIGNALED(st)&&(WTERMSIG(st)==SIGSEGV||WTERMSIG(st)==SIGBUS));
}
static int perms(uintptr_t a,const char *want){FILE *f=fopen("/proc/self/maps","r");if(!f)return -1;char line[512],p[5];unsigned long lo,hi;int rc=-1;while(fgets(line,sizeof line,f))if(sscanf(line,"%lx-%lx %4s",&lo,&hi,p)==3&&a>=lo&&a<hi){rc=strncmp(p,want,3)!=0;break;}fclose(f);return rc;}
int main(int argc,char **argv){if(argc!=3)return 2;struct rlimit lim={0,0};setrlimit(RLIMIT_CORE,&lim);load_mode mode;if(parse_mode(argv[1],&mode))return 2;int fd=open(argv[2],O_RDONLY);struct stat st;if(fd<0||fstat(fd,&st))return 2;file_view f={0};f.size=st.st_size;f.data=mmap(NULL,f.size,PROT_READ,MAP_PRIVATE,fd,0);ueec_view v;loaded_image im;size_t node;if(f.data==MAP_FAILED||parse_ueec(&f,&v)||select_node(&f,&v,NULL,&node)||build_runtime_image(fd,&f,&v,node,mode,&im))return 1;
 if(im.local_count!=5)return 1;
 for(size_t i=0;i<5;i++){int wr=i==2||i==3,ex=i==1;uintptr_t a=im.section_address[i];const char *p=ex?"r-x":wr?"rw-":"r--";
 if(perms(a,p)||check(a,0,1,0)||check(a,1,wr,0)||(i&&check(a,2,ex,wr))){fprintf(stderr,"FAIL section %zu mode %s\n",i,argv[1]);return 1;}printf("PASS section=%zu maps=%s read=PASS write=%s execute=%s\n",i,p,wr?"allowed":"denied",!i?"not-probed":ex?"allowed":"denied");}
 release_image(&im);munmap((void*)f.data,f.size);close(fd);return 0;}
