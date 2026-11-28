#include "ueec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__x86_64__)
#define MACHINE UEEC_MACHINE_X86_64
#elif defined(__i386__)
#define MACHINE UEEC_MACHINE_X86
#elif defined(__aarch64__)
#define MACHINE UEEC_MACHINE_ARM64
#elif defined(__arm__)
#define MACHINE UEEC_MACHINE_ARM32
#elif defined(__riscv) && __riscv_xlen == 32
#define MACHINE UEEC_MACHINE_RISCV32
#elif defined(__riscv) && __riscv_xlen == 64
#define MACHINE UEEC_MACHINE_RISCV64
#elif defined(__powerpc64__)
#define MACHINE UEEC_MACHINE_POWERPC64
#elif defined(__powerpc__)
#define MACHINE UEEC_MACHINE_POWERPC32
#elif defined(__sparc__) && defined(__arch64__)
#define MACHINE UEEC_MACHINE_SPARC64
#elif defined(__sparc__)
#define MACHINE UEEC_MACHINE_SPARC32
#elif defined(__loongarch64) || (defined(__loongarch__) && __loongarch_grlen == 64)
#define MACHINE UEEC_MACHINE_LOONGARCH64
#elif defined(__s390x__)
#define MACHINE UEEC_MACHINE_S390X
#else
#error unsupported architecture
#endif
static uint32_t rd(const unsigned char *p, unsigned n, unsigned be) {
    uint32_t v=0; for(unsigned i=0;i<n;i++) v=(v<<8)|p[be?i:n-1-i]; return v;
}
/* Stage 11.2 validates the fixed header and node table, not the section payload. */
int main(int argc,char **argv) {
    if(argc!=2) return 4;
    FILE *f=fopen(argv[1],"rb"); if(!f) return 4;
    unsigned char h[8], n[6]; unsigned matches=0, selected=0;
    if(fread(h,1,8,f)!=8 || memcmp(h,"_GM",3) || (h[3]&127)!=1) goto invalid;
    unsigned be=h[3]>>7, count=rd(h+4,2,be), names=rd(h+6,2,be);
    if(!count || !names || names>count) goto invalid;
    for(unsigned i=0;i<count;i++) {
        if(fread(n,1,6,f)!=6 || rd(n+4,2,be)>UEEC_FORMAT_SH64) goto invalid;
        uint32_t w=rd(n,4,be);
        if(((w>>7)&7)==UEEC_MACHINE_TYPE_CPU && ((w>>10)&255)==MACHINE &&
           (w&1)==UEEC_HOST_ENDIAN && ((w>>18)&3)==UEEC_SYSTEM_TYPE_GENERAL_OS &&
           ((w>>20)&127)==UEEC_SYSTEM_LINUX && (w&4) && (w&8)) { selected=i; matches++; }
    }
    fclose(f);
    if(!matches) { puts("NO_MATCH"); return 2; }
    if(matches!=1) { puts("AMBIGUOUS"); return 3; }
    printf("SELECTED machine=%u node=%u\n",(unsigned)MACHINE,selected); return 0;
invalid:
    fclose(f); puts("INVALID"); return 4;
}
