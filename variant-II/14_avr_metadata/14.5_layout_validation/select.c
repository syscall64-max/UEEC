#define main legacy_loader_main
#include "container_parser.c"
#undef main
#include "avr_metadata.h"
#include "devices.h"
static const avr_device *profile(const uint8_t *b, unsigned e){
 if(read_u16(b,e)||read_u16(b+2,e)) return NULL;
 for(size_t i=0;i<sizeof devices/sizeof *devices;i++){
  const avr_device *d=devices+i;
  if(memcmp(b+4,d->sig,3)) continue;
  if(b[7]!=d->pc) return NULL;
  for(unsigned j=0;j<7;j++){
   uint32_t v=read_u32(b+8+4*j,e);
   if(j<5 ? v!=d->fields[j] : (v&~d->fields[j])!=0) return NULL;
  }
  return d;
 }
 return NULL;
}
static int choose(const file_view *f,const ueec_view *v,const avr_device *target){
 size_t selected=SIZE_MAX;
 for(size_t n=0;n<v->nodes;n++){
  uint32_t w=node_word(f,v,n);
  if(((w>>7)&7)!=UEEC_MACHINE_TYPE_MPU || ((w>>10)&255)!=UEEC_MACHINE_AVR8 || ((w>>18)&3)!=UEEC_SYSTEM_TYPE_BARE_METAL) continue;
  /* Prototype: bare-metal program, little-endian target code. */
  if((w&31)!=8 || ((w>>20)&127)!=0) continue;
  size_t seen[UEEC_MAX_NODE_SECTIONS],count=0,metadata=0,code=0;
  const avr_device *d=NULL;
  for(size_t j=0;j<v->node_info_count;j++){
   if(mapping_node(f,v,j)!=n) continue;
   size_t k=mapping_section(f,v,j); section_view s; uint64_t off;
   for(size_t t=0;t<count;t++) if(seen[t]==k) return 1;
   if(count>=UEEC_MAX_NODE_SECTIONS) return 1;
   seen[count++]=k;
   if(get_section(f,v,k,&s)||section_file_offset(f,v,k,&off)) return 1;
   if(s.type==UEEC_SECTION_CODE && s.data_size) code++;
   if(s.type==UEEC_SECTION_METADATA){
    if(++metadata!=1 || s.data_size!=UEEC_AVR_META_SIZE || off>f->size || !span_ok(f->size,(size_t)off,UEEC_AVR_META_SIZE)) return 1;
    d=profile(f->data+(size_t)off,v->endian);
    if(!d) return 1;
   }
  }
  if(metadata!=1 || !code) return 1;
  if(d==target){if(selected!=SIZE_MAX) return 1; selected=n;}
 }
 if(selected==SIZE_MAX) return 1;
 printf("SELECTED %zu %s\n",selected,target->name); return 0;
}
#ifndef UEEC_SELECTOR_NO_MAIN
int main(int argc,char **argv){
 if(argc!=3) return 2;
 const avr_device *target=NULL;
 for(size_t i=0;i<sizeof devices/sizeof *devices;i++){
  char sig[7]; snprintf(sig,sizeof sig,"%02x%02x%02x",(unsigned)devices[i].sig[0],(unsigned)devices[i].sig[1],(unsigned)devices[i].sig[2]);
  if(!strcmp(argv[1],sig)) target=devices+i;
 }
 if(!target) return 1;
 int fd=open(argv[2],O_RDONLY); if(fd<0) return 2;
 struct stat st; if(fstat(fd,&st)||st.st_size<=0||(uintmax_t)st.st_size>SIZE_MAX){close(fd);return 1;}
 size_t len=(size_t)st.st_size; void *p=mmap(NULL,len,PROT_READ,MAP_PRIVATE,fd,0); close(fd); if(p==MAP_FAILED) return 2;
 file_view f={p,len}; ueec_view v; int rc=parse_ueec(&f,&v)?1:choose(&f,&v,target); munmap(p,len); return rc;
}

#endif
