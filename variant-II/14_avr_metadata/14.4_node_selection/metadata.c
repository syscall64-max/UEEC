#include "avr_metadata.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static uint32_t get(const uint8_t *p,unsigned n,int be){
    uint32_t v=0; for(unsigned i=0;i<n;i++) v=(v<<8)|p[be?i:n-1-i]; return v;
}
int main(int argc,char **argv){
    if(argc!=3 || (strcmp(argv[1],"le") && strcmp(argv[1],"be"))) return 2;
    int be=!strcmp(argv[1],"be"); uint8_t b[UEEC_AVR_META_SIZE+1];
    FILE *f=fopen(argv[2],"rb"); if(!f) return 2;
    size_t n=fread(b,1,sizeof b,f); int err=ferror(f); fclose(f);
    if(err || n!=UEEC_AVR_META_SIZE || get(b,2,be)!=UEEC_AVR_META_FORMAT32 || get(b+2,2,be)!=UEEC_AVR_META_DESIGN_FIXED) return 1;
    uint32_t a[7]; for(unsigned i=0;i<7;i++) a[i]=get(b+8+4*i,4,be);
    /* Conservative experimental classic AVR profile; PC counts words. */
    if((b[4]==0 && b[5]==0 && b[6]==0) || (b[4]==255 && b[5]==255 && b[6]==255) || !b[7] || b[7]>32 || !a[0] || a[0]%2 || !a[3] || a[3]%2 || a[0]%a[3] || (uint64_t)a[0]/2>(UINT64_C(1)<<b[7])) return 1;
    printf("%02x%02x%02x %u",(unsigned)b[4],(unsigned)b[5],(unsigned)b[6],(unsigned)b[7]);
    for(unsigned i=0;i<7;i++) printf(" %" PRIu32,a[i]);
    puts(""); return 0;
}
