/* Experimental sequential AVR placement, not a new UEEC wire format. */
#define UEEC_SELECTOR_NO_MAIN
#include "select.c"
#undef UEEC_SELECTOR_NO_MAIN
static int place(uint64_t *cursor,uint64_t limit,uint64_t size,unsigned alignment,uint64_t *start){
 uint64_t pad=(alignment-*cursor%alignment)%alignment;
 if(*cursor>limit || pad>limit-*cursor || size>limit-*cursor-pad) return 1;
 *start=*cursor+pad; *cursor=*start+size; return 0;
}
typedef struct { uint8_t *flash,*ram; size_t flash_size,ram_size; uint64_t base,sp,entry,used; } avr_images;
static int layout(const file_view *f,const ueec_view *v,const avr_device *d,size_t n,avr_images *img){
 uint64_t flash=0,base=!strcmp(d->name,"ATmega2560")?512:!strcmp(d->name,"ATmega128")||!strcmp(d->name,"ATmega328P")?256:96;
 uint64_t ram=base,end=base+d->fields[1],stack=32,entry=UINT64_MAX,at;
 size_t stack_count=0;
 /* Reserve stack first, then place code/data by ascending global section index. */
 for(size_t k=1;k<v->section_count;k++){
  int mapped=0; for(size_t j=0;j<v->node_info_count;j++) if(mapping_node(f,v,j)==n&&mapping_section(f,v,j)==k) mapped=1;
  if(!mapped) continue;
  section_view s; if(get_section(f,v,k,&s)) return 1;
  if(s.type==UEEC_SECTION_STACK){if(++stack_count!=1||s.data_size||s.section_size<2) return 1;stack=s.section_size;}
 }
 if(stack>end-base) return 1;
 if(img){img->flash_size=d->fields[0];img->ram_size=d->fields[1];img->base=base;img->sp=end-1;img->flash=malloc(img->flash_size);img->ram=calloc(img->ram_size,1);if(!img->flash||!img->ram)return 1;memset(img->flash,255,img->flash_size);}
 for(size_t k=1;k<v->section_count;k++){
  int mapped=0; for(size_t j=0;j<v->node_info_count;j++) if(mapping_node(f,v,j)==n&&mapping_section(f,v,j)==k) mapped=1;
  if(!mapped) continue;
  section_view s; if(get_section(f,v,k,&s)) return 1;
  uint64_t off,ram_at; if(section_file_offset(f,v,k,&off)||off>f->size||s.data_size>f->size-off)return 1;
  switch(s.type){
  case UEEC_SECTION_CODE:
   if(!s.data_size||s.data_size!=s.section_size||s.section_size%2||place(&flash,d->fields[0],s.section_size,2,&at)) return 1;
   if(entry==UINT64_MAX) entry=at;
   if(img) memcpy(img->flash+at,f->data+off,(size_t)s.data_size);
   break;
  case UEEC_SECTION_CONSTANT_DATA:
   if(s.data_size!=s.section_size||place(&flash,d->fields[0],s.section_size,1,&at)) return 1;
   if(img) memcpy(img->flash+at,f->data+off,(size_t)s.data_size);
   break;
  case UEEC_SECTION_INIT_DATA:
   if(!s.data_size||place(&ram,end-stack,s.section_size,1,&ram_at)||place(&flash,d->fields[0],s.data_size,1,&at)) return 1;
   if(img){memcpy(img->flash+at,f->data+off,(size_t)s.data_size);memcpy(img->ram+ram_at-base,f->data+off,(size_t)s.data_size);}
   break;
  case UEEC_SECTION_UNINIT_DATA:
   if(s.data_size||place(&ram,end-stack,s.section_size,1,&at)) return 1;
   break;
  case UEEC_SECTION_METADATA: case UEEC_SECTION_DEBUG: case UEEC_SECTION_STACK: break;
  default: return 1; /* no EEPROM/relocation/TLS/heap policy yet */
  }
 }
 if(entry==UINT64_MAX||entry%2||entry/2 >= (UINT64_C(1)<<d->pc)) return 1;
 if(img){img->entry=entry;img->used=flash;}
 printf("LAYOUT %s flash=%" PRIu64 " sram=%" PRIu64 " stack=%" PRIu64 " entry=%" PRIu64 "\n",d->name,flash,ram-base,stack,entry);return 0;
}
static int run_layout(int argc,char **argv,avr_images *img){
 if(argc!=3) return 2;
 const avr_device *d=NULL;
 for(size_t i=0;i<sizeof devices/sizeof *devices;i++){char sig[7];snprintf(sig,sizeof sig,"%02x%02x%02x",(unsigned)devices[i].sig[0],(unsigned)devices[i].sig[1],(unsigned)devices[i].sig[2]);if(!strcmp(sig,argv[1])) d=devices+i;}
 if(!d) return 1;
 int fd=open(argv[2],O_RDONLY);if(fd<0)return 2;
 struct stat st;if(fstat(fd,&st)||st.st_size<=0||(uintmax_t)st.st_size>SIZE_MAX){close(fd);return 1;}
 size_t len=(size_t)st.st_size;void *p=mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0);close(fd);if(p==MAP_FAILED)return 2;
 file_view f={p,len};ueec_view v;int rc=1;
 if(!parse_ueec(&f,&v)&&!choose(&f,&v,d)){
  for(size_t j=0;j<v.node_info_count;j++){size_t k=mapping_section(&f,&v,j);section_view s;uint64_t off; uint32_t w=node_word(&f,&v,mapping_node(&f,&v,j));
   if(((w>>7)&7)!=UEEC_MACHINE_TYPE_MPU || ((w>>10)&255)!=UEEC_MACHINE_AVR8 || ((w>>18)&3)!=UEEC_SYSTEM_TYPE_BARE_METAL || (w&31)!=8 || ((w>>20)&127)!=0) continue;
   if(!get_section(&f,&v,k,&s)&&s.type==UEEC_SECTION_METADATA&&s.data_size==UEEC_AVR_META_SIZE&&!section_file_offset(&f,&v,k,&off)&&profile(f.data+(size_t)off,v.endian)==d){rc=layout(&f,&v,d,mapping_node(&f,&v,j),img);break;}
  }
 }
 munmap(p,len);return rc;
}

#ifndef UEEC_LAYOUT_NO_MAIN
int main(int argc,char **argv){return run_layout(argc,argv,NULL);}
#endif
