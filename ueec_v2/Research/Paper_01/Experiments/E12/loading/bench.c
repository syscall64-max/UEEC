#define main original_main
#include "instrumented.c"
#undef main
#include <sys/wait.h>
static long rss_kib(void) { FILE *f=fopen("/proc/self/smaps_rollup","r"); if(!f)return -1; char l[256]; long n=-1; while(fgets(l,sizeof l,f)) if(sscanf(l,"Rss: %ld kB",&n)==1)break; fclose(f);return n; }
int main(int argc,char **argv) {
 if(argc!=6)return 2;
 const char *path=argv[1],*selector=strcmp(argv[3],"-")?argv[3]:NULL; load_mode mode; if(parse_mode(argv[2],&mode))return 2;
 int reps=atoi(argv[4]),expected=atoi(argv[5]); if(reps<1||reps>100000)return 2;
 int fd=open(path,O_RDONLY);struct stat st;if(fd<0||fstat(fd,&st)||st.st_size<=0)return 2;
 file_view f={.data=mmap(NULL,st.st_size,PROT_READ,MAP_PRIVATE,fd,0),.size=(size_t)st.st_size};if(f.data==MAP_FAILED)return 2;
 // Open/map once, cache warm; no process creation inside the measured intervals.
 for(int i=-10;i<reps;i++) {
  ueec_view v; loaded_image im;size_t node;uint64_t a=now_ns();if(parse_ueec(&f,&v))return 3;uint64_t b=now_ns();if(select_node(&f,&v,selector,&node))return 4;uint64_t c=now_ns();
  long rss0=-1,rss1=-1; if(i==-1)rss0=rss_kib();
  uint64_t start=now_ns();if(build_runtime_image(fd,&f,&v,node,mode,&im))return 5;uint64_t end=now_ns();
  if(im.actual_mode!=mode)return 6;
  size_t allocated=0,resident=0;for(size_t j=0;j<im.local_count;j++)allocated+=im.allocation_size[j];
  size_t regions=im.region_count;
  if(i==-1){rss1=rss_kib();long page=sysconf(_SC_PAGESIZE);for(size_t j=0;j<im.region_count;j++){size_t pages=im.regions[j].size/page;unsigned char *vec=calloc(pages,1);if(!vec||mincore(im.regions[j].address,im.regions[j].size,vec))return 7;for(size_t k=0;k<pages;k++)resident+=(vec[k]&1)?page:0;free(vec);}pid_t p=fork();if(p<0)return 8;if(!p){call_raw_entry(im.entry);_exit(254);}int status;if(waitpid(p,&status,0)!=p||!WIFEXITED(status)||WEXITSTATUS(status)!=expected)return 9;fprintf(stderr,"VALIDATED,%zu,%zu,%zu,%ld,%ld\n",allocated,resident,regions,rss0,rss1);}
  uint64_t x=now_ns();release_image(&im);uint64_t y=now_ns();
  if(i>=0)printf("%d,%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%"PRIu64",%zu,%zu\n",i,b-a,c-b,end-start,(b-a)+(c-b)+(end-start),phase[0],phase[1],phase[2],phase[3],phase[4],phase[5],y-x,allocated,regions);
 }
 munmap((void*)f.data,f.size);close(fd);return 0;
}
