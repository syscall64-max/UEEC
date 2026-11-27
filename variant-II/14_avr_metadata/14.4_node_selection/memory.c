#include "avr_metadata.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static uint32_t get(const uint8_t *p,unsigned n,int be){
    uint32_t v=0; for(unsigned i=0;i<n;i++) v=(v<<8)|p[be?i:n-1-i]; return v;
}
/* Prototype capability bits: not AVR vendor identifiers. */
#include "devices.h"
static int read_device(int argc,char **argv,const avr_device **out){
    if(argc<3 || (strcmp(argv[1],"le") && strcmp(argv[1],"be"))) return 2;
    int be=!strcmp(argv[1],"be"); uint8_t b[UEEC_AVR_META_SIZE+1];
    FILE *f=fopen(argv[2],"rb"); if(!f) return 2;
    size_t n=fread(b,1,sizeof b,f); int err=ferror(f); fclose(f);
    if(err || n!=UEEC_AVR_META_SIZE || get(b,2,be)!=UEEC_AVR_META_FORMAT32 || get(b+2,2,be)!=UEEC_AVR_META_DESIGN_FIXED) return 1;
    uint32_t a[7]; for(unsigned i=0;i<7;i++) a[i]=get(b+8+4*i,4,be);
    /* Conservative experimental classic AVR profile; PC counts words. */
    if((b[4]==0 && b[5]==0 && b[6]==0) || (b[4]==255 && b[5]==255 && b[6]==255) || !b[7] || b[7]>32 || !a[0] || a[0]%2 || !a[3] || a[3]%2 || a[0]%a[3] || (uint64_t)a[0]/2>(UINT64_C(1)<<b[7])) return 1;
    const avr_device *d=NULL;
    for(size_t i=0;i<sizeof devices/sizeof devices[0];i++)
        if(!memcmp(b+4,devices[i].sig,3)) { d=&devices[i]; break; }
    if(!d || b[7]!=d->pc) return 1;
    for(unsigned i=0;i<5;i++) if(a[i]!=d->fields[i]) return 1;
    if((a[5]&~d->fields[5]) || (a[6]&~d->fields[6])) return 1;
    *out=d; return 0;
}
#include <stdlib.h>
#include <errno.h>
static int num(const char *s,uint64_t *out){
    if(!*s) return 0;
    for(const char *p=s;*p;p++) if(*p<'0'||*p>'9') return 0;
    errno=0; char *end; unsigned long long v=strtoull(s,&end,10);
    if(errno||*end||v>UINT64_MAX) return 0;
    *out=(uint64_t)v; return 1;
}
int main(int argc,char **argv){
    const avr_device *d; int rc=read_device(argc,argv,&d); if(rc) return rc;
    /* Classic AVR data-space addresses, not physical memory addresses. */
    uint64_t base=!strcmp(d->name,"ATmega2560")?512:!strcmp(d->name,"ATmega128")||!strcmp(d->name,"ATmega328P")?256:96;
    uint64_t flash=d->fields[0],sram=d->fields[1],eep=d->fields[2],page=d->fields[3];
    if(argc==4&&!strcmp(argv[3],"info")){
        printf("%s %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",d->name,base,base+sram,flash/2,flash/page,eep);return 0;
    }
    if(argc!=6) return 2;
    uint64_t a,n,lo=0,hi=0; if(!num(argv[4],&a)||!num(argv[5],&n)||!n) return 1;
    if(!strcmp(argv[3],"flash")) hi=flash;
    else if(!strcmp(argv[3],"word")) hi=flash/2;
    else if(!strcmp(argv[3],"page")) hi=flash/page;
    else if(!strcmp(argv[3],"sram")){lo=base;hi=base+sram;}
    else if(!strcmp(argv[3],"eeprom")) hi=eep;
    else if(!strcmp(argv[3],"register")) hi=32;
    else if(!strcmp(argv[3],"io")){lo=32;hi=96;}
    else if(!strcmp(argv[3],"extended-io")){lo=96;hi=base;}
    else return 2;
    if(a<lo||a>=hi||n>hi-a) return 1;
    uint64_t scale=!strcmp(argv[3],"word")?2:!strcmp(argv[3],"page")?page:1;
    printf("%" PRIu64 " %" PRIu64 "\n",a*scale,n*scale);return 0;
}
