#define main previous_stage_main
#include "load.c"
#undef main
/* Experimental IMPORT layout, not a standardized UEEC wire format. */
typedef struct {const uint8_t *p; size_t n,pos; unsigned e;} cursor_t;
static int num(cursor_t *c,size_t w,uint64_t *v) {
 if(!span_ok(c->n,c->pos,w))return -1;
 *v=w==1?c->p[c->pos]:read_width(c->p+c->pos,c->e,w);c->pos+=w;return 0;
}
static int name(cursor_t *c,const uint8_t **p,size_t *n) {
 uint64_t k;if(num(c,2,&k)||!k||!span_ok(c->n,c->pos,(size_t)k))return -1;
 *p=c->p+c->pos;*n=(size_t)k;c->pos+=*n;
 return memchr(*p,0,*n)?-1:0;
}
typedef struct {size_t sec,off;} slot_t;
static int cmp_slot(const void *a,const void *b) {
 const slot_t *x=a,*y=b; if(x->sec!=y->sec)return x->sec<y->sec?-1:1;
 return (x->off>y->off)-(x->off<y->off);
}
static int parse_import(cursor_t c,const loaded_image *im) {
 uint64_t fmt,design,libs; if(num(&c,2,&fmt)||num(&c,2,&design)||fmt>2||design!=0)return -1;
 size_t w=(size_t)2<<fmt, count=0, cap=0;slot_t *slots=NULL;int rc=-1;
 if(num(&c,w,&libs)||!libs||libs>c.n)goto done;
 for(uint64_t l=0;l<libs;l++) {
  const uint8_t *lib,*sym=NULL;size_t ln,sn=0;uint64_t entries;
  if(name(&c,&lib,&ln)||num(&c,w,&entries)||!entries||entries>c.n)goto done;
  for(uint64_t i=0;i<entries;i++) {
   uint64_t kind,sec,off,ord=0;
   if(num(&c,1,&kind)||kind>1||num(&c,w,&sec)||num(&c,w,&off))goto done;
   if(kind==0){if(name(&c,&sym,&sn))goto done;}else if(num(&c,w,&ord))goto done;
   if(sec>=im->local_count-1||off>SIZE_MAX)goto done;
   const section_view *s=&im->section[sec+1];
   if(s->type!=UEEC_SECTION_INIT_DATA && s->type!=UEEC_SECTION_UNINIT_DATA)goto done;
   if(off>s->section_size||sizeof(uintptr_t)>s->section_size-off||off%sizeof(uintptr_t))goto done;
   if(count==cap){size_t next=cap?cap*2:16;if(next<cap||next>SIZE_MAX/sizeof(*slots))goto done;void *p=realloc(slots,next*sizeof(*slots));if(!p)goto done;slots=p;cap=next;}
   slots[count++]=(slot_t){(size_t)sec,(size_t)off};
   printf("IMPORT library=%.*s section=%" PRIu64 " offset=%" PRIu64 " ",(int)ln,(const char *)lib,sec,off);
   if(kind==0)printf("name=%.*s\n",(int)sn,(const char *)sym);else printf("ordinal=%" PRIu64 "\n",ord);
  }
 }
 if(c.pos!=c.n)goto done;
 qsort(slots,count,sizeof(*slots),cmp_slot);
 for(size_t i=1;i<count;i++)if(slots[i].sec==slots[i-1].sec&&slots[i].off-slots[i-1].off<sizeof(uintptr_t))goto done;
 printf("VALID imports=%zu\n",count);rc=0;
 done:free(slots);return rc;
}
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 load_mode mode;if(parse_mode(argv[1],&mode))return 2;
 int fd=open(argv[2],O_RDONLY);struct stat st;if(fd<0)return 2;
 if(fstat(fd,&st)||st.st_size<=0||(uintmax_t)st.st_size>SIZE_MAX){close(fd);return 2;}
 file_view f={mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0),(size_t)st.st_size};
 if(f.data==MAP_FAILED){close(fd);return 2;}
 ueec_view v;loaded_image im={0};size_t node;int rc=1;
 if(parse_ueec(&f,&v)||select_node(&f,&v,NULL,&node)||build_runtime_image(fd,&f,&v,node,mode,&im)||im.import_count!=1)goto done;
 section_view s;uint64_t off;
 if(get_section(&f,&v,im.import_global[0],&s)||section_file_offset(&f,&v,im.import_global[0],&off)||off>SIZE_MAX||s.data_size>SIZE_MAX||!span_ok(f.size,(size_t)off,(size_t)s.data_size)||parse_import((cursor_t){f.data+(size_t)off,(size_t)s.data_size,0,v.endian},&im))goto done;
 rc=0;
 done:if(rc)fprintf(stderr,"invalid import or image\n");release_image(&im);munmap((void *)f.data,f.size);close(fd);return rc;
}
