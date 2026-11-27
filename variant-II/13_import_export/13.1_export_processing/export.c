#define main previous_stage_main
#include "load.c"
#undef main
/* Existing export wire convention: low five bits, resident index excluding HEADER. */
typedef struct { const uint8_t *p; size_t n,pos; unsigned e; } cursor_t;
static int take(cursor_t *c,size_t n,const uint8_t **p) {
 if (!span_ok(c->n,c->pos,n)) return -1;
 *p=c->p+c->pos; c->pos+=n; return 0;
}
static int number(cursor_t *c,size_t w,uint64_t *v) {
 const uint8_t *p; if(take(c,w,&p)) return -1;
 *v=w==1?*p:read_width(p,c->e,w); return 0;
}
static int cmp(const uint8_t *a,size_t an,const uint8_t *b,size_t bn) {
 int r=memcmp(a,b,an<bn?an:bn); return r?r:(an>bn)-(an<bn);
}
typedef struct { const char *name; uint64_t ordinal; int byord,found; size_t section; uint64_t offset; } query_t;
static int entry(const loaded_image *im,query_t *q,uint64_t sec,uint64_t off,const uint8_t *name,size_t len,uint64_t ord,int byord) {
 if(sec>=im->local_count-1 || off>=im->section[sec+1].section_size) return -1;
 if(im->section[sec+1].type==UEEC_SECTION_CODE && off>=im->section[sec+1].data_size) return -1;
 if(byord==q->byord && (byord?ord==q->ordinal:strlen(q->name)==len && !memcmp(name,q->name,len))) {
  if(q->found) return -1;
  q->found=1; q->section=(size_t)sec+1; q->offset=off;
 }
 return 0;
}
static int table(cursor_t c,const loaded_image *im,query_t *q) {
 uint64_t f,d,f2,n; if(number(&c,2,&f)||number(&c,2,&d)||number(&c,2,&f2)||f>2||d>2||f!=f2) return -1;
 size_t w=(size_t)2<<f; if(number(&c,w,&n)||n>c.n) return -1;
 if(d==0) {
  const uint8_t *headers; size_t bytes; if(multiply_size((size_t)n,2+w,&bytes)||take(&c,bytes,&headers)) return -1;
  uint64_t prevlen=0;
  for(size_t g=0;g<n;g++) {
   uint64_t len=read_u16(headers+g*(2+w),c.e), k=read_width(headers+g*(2+w)+2,c.e,w);
   if(!len||len<=prevlen||!k||k>c.n) return -1;
   prevlen=len; const uint8_t *infos,*names;
   if(multiply_size((size_t)k,1+w,&bytes)||take(&c,bytes,&infos)||multiply_size((size_t)k,(size_t)len,&bytes)||take(&c,bytes,&names)) return -1;
   for(size_t i=0;i<k;i++) {
    const uint8_t *a=infos+i*(1+w),*name=names+i*len;
    if((*a&224)||(i&&memcmp(name-len,name,(size_t)len)>=0)||entry(im,q,*a,read_width(a+1,c.e,w),name,(size_t)len,0,0)) return -1;
   }
  }
 } else if(d==1) {
  const uint8_t *infos; size_t bytes; if(multiply_size((size_t)n,2+2*w,&bytes)||take(&c,bytes,&infos)) return -1;
  const uint8_t *prev=NULL; size_t plen=0, blob=c.n-c.pos;
  for(size_t i=0;i<n;i++) {
   const uint8_t *a=infos+i*(2+2*w); uint16_t b=read_u16(a,c.e); size_t len=b>>5; uint64_t off=read_width(a+2+w,c.e,w);
   if(!len||off>blob||len>blob-off) return -1;
   const uint8_t *name=c.p+c.pos+(size_t)off;
   if((prev&&cmp(prev,plen,name,len)>=0)||entry(im,q,b&31,read_width(a+2,c.e,w),name,len,0,0)) return -1;
   prev=name; plen=len;
  }
  c.pos=c.n;
 } else {
  uint64_t prev=0;
  for(size_t i=0;i<n;i++) {
   uint64_t sec,off,ord; if(number(&c,1,&sec)||number(&c,w,&off)||number(&c,w,&ord)||(sec&224)||(i&&ord<=prev)||entry(im,q,sec,off,NULL,0,ord,1)) return -1;
   prev=ord;
  }
 }
 return c.pos==c.n?0:-1;
}
int main(int argc,char **argv) {
 if(argc!=5) {fprintf(stderr,"usage: %s MODE FILE name|ordinal QUERY\n",argv[0]);return 2;}
 load_mode mode; if(parse_mode(argv[1],&mode)) return 2;
 query_t q={0}; q.name=argv[4];
 if(!strcmp(argv[3],"ordinal")) {char *end; errno=0; if(!*q.name||*q.name=='-')return 2; q.ordinal=strtoull(q.name,&end,10); if(errno||*end)return 2; q.byord=1;} else if(strcmp(argv[3],"name"))return 2;
 int fd=open(argv[2],O_RDONLY); struct stat st; if(fd<0)return 2;
 if(fstat(fd,&st)||st.st_size<=0||(uintmax_t)st.st_size>SIZE_MAX){close(fd);return 2;}
 file_view f={mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0),(size_t)st.st_size};
 if(f.data==MAP_FAILED){close(fd);return 2;}
 ueec_view v; loaded_image im={0}; size_t node; int rc=1;
 if(parse_ueec(&f,&v)||select_node(&f,&v,NULL,&node)||build_runtime_image(fd,&f,&v,node,mode,&im))goto done;
 if(!im.export_count)goto done;
 for(size_t i=0;i<im.export_count;i++) {
  section_view s; uint64_t off;
  if(get_section(&f,&v,im.export_global[i],&s)||section_file_offset(&f,&v,im.export_global[i],&off)||off>SIZE_MAX||s.data_size>SIZE_MAX||!span_ok(f.size,(size_t)off,(size_t)s.data_size)||table((cursor_t){f.data+(size_t)off,(size_t)s.data_size,0,v.endian},&im,&q))goto done;
 }
 if(!q.found){puts("NOT_FOUND");rc=3;goto done;}
 printf("FOUND node=%zu section=%zu offset=%" PRIu64 " byte=%u\n",node,q.section-1,q.offset,(unsigned)*(const uint8_t *)(im.section_address[q.section]+(size_t)q.offset));rc=0;
 done: if(rc==1)fprintf(stderr,"invalid export or image\n");release_image(&im);munmap((void *)f.data,f.size);close(fd);return rc;
}
