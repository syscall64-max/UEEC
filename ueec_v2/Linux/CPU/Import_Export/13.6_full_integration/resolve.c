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
static int num(cursor_t *c,size_t w,uint64_t *v) {
 if(!span_ok(c->n,c->pos,w))return -1;
 *v=w==1?c->p[c->pos]:read_width(c->p+c->pos,c->e,w);c->pos+=w;return 0;
}
static int name(cursor_t *c,const uint8_t **p,size_t *n) {
 uint64_t k;if(num(c,2,&k)||!k||!span_ok(c->n,c->pos,(size_t)k))return -1;
 *p=c->p+c->pos;*n=(size_t)k;c->pos+=*n;
 return memchr(*p,0,*n)?-1:0;
}
typedef struct {int fd; file_view f; ueec_view v; loaded_image im;} module;
static void unload(module *m){release_image(&m->im);if(m->f.data && m->f.data!=MAP_FAILED)munmap((void *)m->f.data,m->f.size);if(m->fd>=0)close(m->fd);}
static int load(module *m,const char *path,load_mode mode){struct stat st;size_t node;m->fd=open(path,O_RDONLY);if(m->fd<0||fstat(m->fd,&st)||st.st_size<=0||(uintmax_t)st.st_size>SIZE_MAX)return -1;m->f.size=(size_t)st.st_size;m->f.data=mmap(NULL,m->f.size,PROT_READ,MAP_PRIVATE,m->fd,0);return m->f.data==MAP_FAILED||parse_ueec(&m->f,&m->v)||select_node(&m->f,&m->v,NULL,&node)||build_runtime_image(m->fd,&m->f,&m->v,node,mode,&m->im)?-1:0;}
static int section_cursor(module *m,size_t index,cursor_t *c){section_view s;uint64_t off;if(get_section(&m->f,&m->v,index,&s)||section_file_offset(&m->f,&m->v,index,&off)||off>SIZE_MAX||s.data_size>SIZE_MAX||!span_ok(m->f.size,(size_t)off,(size_t)s.data_size))return -1;*c=(cursor_t){m->f.data+(size_t)off,(size_t)s.data_size,0,m->v.endian};return 0;}
typedef struct {const char *name;module m;} library;
typedef struct {size_t sec,off;uintptr_t value;} binding;
static int bind_imports(module *app,library *libs,size_t nl,int execute){binding *b=NULL;size_t n=0,cap=0;int rc=-1;loaded_image *im=&app->im;
 for(size_t j=0;j<im->import_count;j++){cursor_t c;uint64_t fmt,d,count;if(section_cursor(app,im->import_global[j],&c)||num(&c,2,&fmt)||num(&c,2,&d)||fmt>2||d)goto done;size_t w=(size_t)2<<fmt;if(num(&c,w,&count)||!count||count>c.n)goto done;
 for(uint64_t l=0;l<count;l++){const uint8_t *ln;size_t len;uint64_t entries;if(name(&c,&ln,&len)||num(&c,w,&entries)||!entries||entries>c.n)goto done;module *lib=NULL;for(size_t k=0;k<nl;k++)if(strlen(libs[k].name)==len&&!memcmp(ln,libs[k].name,len)){if(lib)goto done;lib=&libs[k].m;}if(!lib)goto done;
 for(uint64_t i=0;i<entries;i++){uint64_t kind,sec,off,ord=0;const uint8_t *sym;size_t sn;char *str=NULL;if(num(&c,1,&kind)||kind>1||num(&c,w,&sec)||num(&c,w,&off)||sec>=im->local_count-1||off>SIZE_MAX)goto done;section_view *s=&im->section[sec+1];if((s->type!=UEEC_SECTION_INIT_DATA&&s->type!=UEEC_SECTION_UNINIT_DATA)||off>s->section_size||sizeof(uintptr_t)>s->section_size-off||off%sizeof(uintptr_t))goto done;
 if(kind){if(num(&c,w,&ord))goto done;}else{if(name(&c,&sym,&sn))goto done;str=malloc(sn+1);if(!str)goto done;memcpy(str,sym,sn);str[sn]=0;}
 query_t q={.name=str,.ordinal=ord,.byord=(int)kind};int bad=0;for(size_t k=0;k<lib->im.export_count;k++){cursor_t ec;if(section_cursor(lib,lib->im.export_global[k],&ec)||table(ec,&lib->im,&q)){bad=1;break;}}free(str);if(bad||!q.found||(execute==1 && lib->im.section[q.section].type!=UEEC_SECTION_CODE) || (execute==2 && (lib->im.section[q.section].type != (off==0?UEEC_SECTION_CONSTANT_DATA:off==8?UEEC_SECTION_INIT_DATA:UEEC_SECTION_UNINIT_DATA))))goto done;
 for(size_t k=0;k<n;k++)if(b[k].sec==sec+1&&b[k].off==off)goto done;
 if(n==cap){size_t next=cap?cap*2:16;if(next<cap||next>SIZE_MAX/sizeof(*b))goto done;void *p=realloc(b,next*sizeof(*b));if(!p)goto done;b=p;cap=next;}b[n++]=(binding){(size_t)sec+1,(size_t)off,lib->im.section_address[q.section]+(size_t)q.offset};
 } }if(c.pos!=c.n)goto done;}
 if(!n)goto done;
 /* Reject import slots overlapping already applied relocation ranges. */
 for(size_t j=0;j<im->reloc_count;j++){cursor_t c;if(section_cursor(app,im->reloc_global[j],&c)||c.n<4)goto done;size_t total=0;patch_range *r=calloc(c.n/2,sizeof(*r));if(!r)goto done;int bad=decode_relocations(c.p,c.n,c.e,im,r,&total,0);for(size_t k=0;!bad&&k<total;k++)for(size_t i=0;i<n;i++)if(r[k].target==b[i].sec&&r[k].start<b[i].off+sizeof(uintptr_t)&&b[i].off<r[k].end)bad=1;free(r);if(bad)goto done;}
 for(size_t i=0;i<n;i++)memcpy((void *)(im->section_address[b[i].sec]+b[i].off),&b[i].value,sizeof(uintptr_t));
 for(size_t i=0;i<n;i++){uintptr_t v;memcpy(&v,(void *)(im->section_address[b[i].sec]+b[i].off),sizeof v);if(v!=b[i].value)goto done;printf("BOUND section=%zu offset=%zu byte=%u\n",b[i].sec-1,b[i].off,(unsigned)*(const uint8_t *)v);}printf("RESOLVED imports=%zu\n",n);
 if(execute){
  if(n!=3 || im->local_count<4 || im->section[3].type!=UEEC_SECTION_INIT_DATA || im->section[3].section_size<24)goto done;
  for(size_t k=0;k<3;k++){int found=0;for(size_t i=0;i<n;i++)if(b[i].sec==3 && b[i].off==k*8)found=1;if(!found)goto done;}
  for(int i=0;i<3;i++){int result=execute_entry(im->entry,(void *)im->section_address[3],NULL,NULL);if(execute==1){printf("CALL result=%d\n",result);if(result!=96)goto done;}else{printf("DATA result=%d\n",result);if(result!=172+52*i)goto done; for(size_t k=0;k<n;k++){unsigned expected=b[k].off==0?100:b[k].off==8?30+10*i:42+42*i;if(*(const uint8_t *)b[k].value!=expected)goto done;}}}
 }rc=0;
 done:free(b);return rc;}
/* DFS over a fixed, explicitly supplied module registry. Each image is mapped once. */
static int visit(module *m,library *libs,size_t nl,unsigned char *state,size_t idx){
 if(state[idx]==2)return 0;
 if(state[idx]==1)return -1;
 state[idx]=1;
 for(size_t j=0;j<m->im.import_count;j++){
  cursor_t c;uint64_t f,d,n;
  if(section_cursor(m,m->im.import_global[j],&c)||num(&c,2,&f)||num(&c,2,&d)||f>2||d)return -1;
  size_t w=(size_t)2<<f;
  if(num(&c,w,&n)||!n||n>c.n)return -1;
  for(uint64_t l=0;l<n;l++){
   const uint8_t *ln,*sn;size_t len,slen;uint64_t k,kind,sec,off,ord;
   if(name(&c,&ln,&len)||num(&c,w,&k)||!k||k>c.n)return -1;
   size_t match=nl;
   for(size_t i=0;i<nl;i++)if(strlen(libs[i].name)==len&&!memcmp(ln,libs[i].name,len)){match=i;break;}
   if(match==nl||visit(&libs[match].m,libs,nl,state,match))return -1;
   for(uint64_t a=0;a<k;a++){
    if(num(&c,1,&kind)||kind>1||num(&c,w,&sec)||num(&c,w,&off))return -1;
    if(kind?num(&c,w,&ord):name(&c,&sn,&slen))return -1;
   }
  }
  if(c.pos!=c.n)return -1;
 }
 if(m->im.import_count&&bind_imports(m,libs,nl,0))return -1;
 state[idx]=2;return 0;
}
/* Test ABI: three imported pointers in resident INIT_DATA, shared by all callers. */
static int shared_test(module *app,library *libs,size_t n){
 if(n!=3)return -1;
 module *mods[3]={&libs[0].m,&libs[1].m,app};
 uintptr_t slots[3]={0};
 for(size_t i=0;i<3;i++){
  loaded_image *im=&mods[i]->im;
  if(im->local_count<4||im->section[3].type!=UEEC_SECTION_INIT_DATA||im->section[3].section_size<24)return -1;
  for(size_t j=0;j<3;j++){
   uintptr_t v;memcpy(&v,(void *)(im->section_address[3]+j*8),sizeof v);
   if(!v||(i&&v!=slots[j]))return -1;
   slots[j]=v;
  }
 }
 uintptr_t relocated;memcpy(&relocated,(void *)(app->im.section_address[3]+40),sizeof relocated);
 if(relocated!=app->im.section_address[2])return -1;
 /* Confirm pointers belong to the intended common data sections before execution. */
 loaded_image *common=&libs[2].m.im;
 if(common->local_count<5||common->section[2].type!=UEEC_SECTION_CONSTANT_DATA||common->section[3].type!=UEEC_SECTION_INIT_DATA||common->section[4].type!=UEEC_SECTION_UNINIT_DATA)return -1;
 for(size_t j=0;j<3;j++)if(slots[j]!=common->section_address[j+2])return -1;
 for(size_t i=0;i<3;i++){
  loaded_image *im=&mods[i]->im;
  int r=execute_entry(im->entry,(void *)im->section_address[3],NULL,NULL);
  printf("SHARED result=%d\n",r);
  if(r!=172+52*(int)i||*(uint8_t *)slots[0]!=100||*(uint8_t *)slots[1]!=30+10*i||*(uint8_t *)slots[2]!=42+42*i)return -1;
 }
 return 0;
}
int main(int argc,char **argv){int protection=argc>1&&!strcmp(argv[1],"--graph-protect");int graph=protection||(argc>1&&!strcmp(argv[1],"--graph"));if(graph){argc--;argv++;}int execute=argc>1&&!strcmp(argv[1],"--data")?2:argc>1&&!strcmp(argv[1],"--execute");if(execute){argc--;argv++;}if(argc<5||(argc-3)%2||(graph&&(argc-3)/2>128))return 2;load_mode mode;if(parse_mode(argv[1],&mode))return 2;module app={.fd=-1};size_t n=(size_t)(argc-3)/2;library *libs=calloc(n,sizeof(*libs));if(!libs)return 2;for(size_t i=0;i<n;i++)libs[i].m.fd=-1;int rc=1;if(load(&app,argv[2],mode))goto done;for(size_t i=0;i<n;i++){libs[i].name=argv[3+2*i];for(size_t j=0;j<i;j++)if(!strcmp(libs[i].name,libs[j].name))goto done;if(load(&libs[i].m,argv[4+2*i],mode)||(!graph&&libs[i].m.im.import_count))goto done;}if(graph){unsigned char *state=calloc(n+1,1);if(state){if(!visit(&app,libs,n,state,n)&&!shared_test(&app,libs,n)&&(!protection||!probe_protection(&libs[2].m.im)))rc=0;free(state);}}else if(!bind_imports(&app,libs,n,execute))rc=0;done:if(rc)fprintf(stderr,"resolution failed\n");unload(&app);for(size_t i=0;i<n;i++)unload(&libs[i].m);free(libs);return rc;}
