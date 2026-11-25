#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "ueec.h"

#if defined(__x86_64__)
# define HOST_MACHINE UEEC_MACHINE_X86_64
#elif defined(__i386__)
# define HOST_MACHINE UEEC_MACHINE_X86
#elif defined(__aarch64__)
# define HOST_MACHINE UEEC_MACHINE_ARM64
#elif defined(__arm__)
# define HOST_MACHINE UEEC_MACHINE_ARM32
#elif defined(__riscv) && (__riscv_xlen == 64)
# define HOST_MACHINE UEEC_MACHINE_RISCV64
#elif defined(__riscv) && (__riscv_xlen == 32)
# define HOST_MACHINE UEEC_MACHINE_RISCV32
#elif defined(__powerpc64__)
# define HOST_MACHINE UEEC_MACHINE_POWERPC64
#elif defined(__powerpc__)
# define HOST_MACHINE UEEC_MACHINE_POWERPC32
#elif defined(__sparc__) && defined(__arch64__)
# define HOST_MACHINE UEEC_MACHINE_SPARC64
#elif defined(__sparc__)
# define HOST_MACHINE UEEC_MACHINE_SPARC32
#elif defined(__s390x__)
# define HOST_MACHINE UEEC_MACHINE_S390X
#elif defined(__loongarch64)
# define HOST_MACHINE UEEC_MACHINE_LOONGARCH64
#else
# error "unknown host architecture"
#endif

typedef struct { int machine_type, machine, section_count; } node_t;
typedef struct { int n, s; } info_t;
typedef struct { void *addr; size_t map_size; int global_section; int type; } resident_t;
typedef struct { void *addr; size_t map_size; int global_section; } temporary_t;

static uint16_t rd16(const uint8_t *p, unsigned e) { uint16_t v; memcpy(&v,p,2); return e==UEEC_HOST_ENDIAN?v:__builtin_bswap16(v); }
static uint32_t rd32(const uint8_t *p, unsigned e) { uint32_t v; memcpy(&v,p,4); return e==UEEC_HOST_ENDIAN?v:__builtin_bswap32(v); }
static uint64_t rd64(const uint8_t *p, unsigned e) { uint64_t v; memcpy(&v,p,8); return e==UEEC_HOST_ENDIAN?v:__builtin_bswap64(v); }

static int pread_full(int fd, void *buf, size_t n, off_t off) {
    uint8_t *p = buf;
    while (n) {
        ssize_t r = pread(fd, p, n, off);
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (r == 0) { errno = EIO; return -1; }
        p += (size_t)r; off += r; n -= (size_t)r;
    }
    return 0;
}

static size_t page_round(size_t n, size_t page) { return (n + page - 1) & ~(page - 1); }

static int apply_reloc(void **local_base, const size_t *local_size, int local_count,
                       int target, int source, size_t patch_offset, int type) {
    size_t width = type == 1 ? 8u : 4u;
    if (target < 0 || target >= local_count || source < 0 || source >= local_count ||
        patch_offset > local_size[target] || width > local_size[target] - patch_offset) {
        fprintf(stderr, "invalid relocation target/source/offset\n");
        return -1;
    }
    uint8_t *target_addr = (uint8_t *)local_base[target] + patch_offset;
    uintptr_t source_base = (uintptr_t)local_base[source];
    if (type == 1) {
        uint64_t addend; memcpy(&addend, target_addr, 8);
        uint64_t patched = (uint64_t)source_base + addend;
        memcpy(target_addr, &patched, 8);
    } else if (type == 0) {
        uint32_t addend; memcpy(&addend, target_addr, 4);
        uint32_t patched = (uint32_t)(source_base + addend);
        memcpy(target_addr, &patched, 4);
    } else {
        fprintf(stderr, "unsupported relocation type: %d\n", type);
        return -1;
    }
    printf("  RELOC: local[%d]+0x%zx <- local[%d] + addend (%s)\n",
           target, patch_offset, source, type ? "ABS64" : "ABS32");
    return 0;
}

static int process_reloc(const uint8_t *rp, size_t rsize, unsigned e,
                         void **base, const size_t *sizes, int local_count) {
    size_t p = 0;
#define NEED(n) do { if ((n) > rsize - p) { fprintf(stderr,"truncated relocation section\n"); return -1; } } while (0)
    NEED(8);
    uint16_t format = rd16(rp+p,e); p+=2;
    uint16_t design = rd16(rp+p,e); p+=2;
    uint16_t couple = rd16(rp+p,e); p+=2;
    int target = couple & 0x1f;
    int source = (couple >> 5) & 0x1f;
    uint16_t count = rd16(rp+p,e); p+=2;

    if (design == 0) {
        for (uint16_t i=0; i<count; ++i) {
            NEED(2); uint16_t v=rd16(rp+p,e); p+=2;
            if (apply_reloc(base,sizes,local_count,target,source,v&0xfffu,(v>>12)&0xf)) return -1;
        }
    } else if (design == 1 || design == 2) {
        int w = format==0 ? 2 : format==1 ? 4 : format==2 ? 8 : 0;
        if (!w) { fprintf(stderr,"unsupported relocation format: %u\n",format); return -1; }
        for (uint16_t b=0; b<count; ++b) {
            uint64_t index, bc;
            NEED((size_t)w*2u);
            if (w==2) { index=rd16(rp+p,e); p+=2; bc=rd16(rp+p,e); p+=2; }
            else if (w==4) { index=rd32(rp+p,e); p+=4; bc=rd32(rp+p,e); p+=4; }
            else { index=rd64(rp+p,e); p+=8; bc=rd64(rp+p,e); p+=8; }
            for (uint64_t i=0; i<bc; ++i) {
                uint64_t delta; int type;
                NEED((size_t)w);
                if (w==2) { uint16_t v=rd16(rp+p,e);p+=2;delta=v&0xfffu;type=(v>>12)&0xf; }
                else if (w==4) { uint32_t v=rd32(rp+p,e);p+=4;delta=v&0x0fffffffu;type=(int)((v>>28)&0xfu); }
                else { uint64_t v=rd64(rp+p,e);p+=8;delta=v&UINT64_C(0x0fffffffffffffff);type=(int)((v>>60)&0xf); }
                if (index > SIZE_MAX-delta || apply_reloc(base,sizes,local_count,target,source,(size_t)(index+delta),type)) return -1;
            }
        }
    } else { fprintf(stderr,"unsupported reloc design: %u\n",design); return -1; }
    return 0;
#undef NEED
}

int main(int argc, char **argv) {
    const char *path=NULL, *mode="monolithic";
    for (int i=1;i<argc;i++) { if (!strncmp(argv[i],"--L=",4)) mode=argv[i]+4; else path=argv[i]; }
    if (!path || (strcmp(mode,"monolithic") && strcmp(mode,"scattered"))) {
        fprintf(stderr,"usage: %s [--L=monolithic|scattered] FILE.ueec\n",argv[0]); return 1;
    }

    int fd=open(path,O_RDONLY); if(fd<0){perror("open");return 1;}
    struct stat st; if(fstat(fd,&st)<0){perror("fstat");return 1;}
    uint8_t h[8]; if(pread_full(fd,h,sizeof h,0)){perror("pread header");return 1;}
    if(h[0]!=UEEC_MAGIC_0||h[1]!=UEEC_MAGIC_1||h[2]!=UEEC_MAGIC_2){fprintf(stderr,"invalid magic\n");return 1;}
    unsigned e=(h[3]>>7)&1u, nodes=rd16(h+4,e);
    if(!nodes){fprintf(stderr,"zero nodes\n");return 1;}

    size_t node_bytes=(size_t)nodes*6u;
    uint8_t *nb=malloc(node_bytes); if(!nb||pread_full(fd,nb,node_bytes,8)){perror("node headers");return 1;}
    node_t *node=calloc(nodes,sizeof *node); long total_info=0;
    for(unsigned i=0;i<nodes;i++){ uint32_t v=rd32(nb+i*6u,e); node[i].machine_type=(int)((v>>7)&7u); node[i].machine=(int)((v>>10)&0xffu); node[i].section_count=(int)(((v>>27)&0x1fu)+1u); total_info+=node[i].section_count-1; }
    free(nb);

    off_t info_pos=8+(off_t)node_bytes; size_t info_bytes=(size_t)total_info*6u;
    uint8_t *ib=malloc(info_bytes); if(!ib||pread_full(fd,ib,info_bytes,info_pos)){perror("node info");return 1;}
    info_t *info=calloc((size_t)total_info,sizeof *info); long max_section=0;
    for(long i=0;i<total_info;i++){ info[i].n=rd16(ib+i*6u,e); info[i].s=(int)rd32(ib+i*6u+2,e); if(info[i].s>max_section)max_section=info[i].s; }
    free(ib);
    long total_sections=max_section+1; if(total_sections<=0){fprintf(stderr,"no sections\n");return 1;}

    off_t sh_pos=info_pos+(off_t)info_bytes; size_t sh_bytes=(size_t)total_sections*6u;
    uint8_t *sb=malloc(sh_bytes); if(!sb||pread_full(fd,sb,sh_bytes,sh_pos)){perror("section headers");return 1;}
    int *sec_type=calloc((size_t)total_sections,sizeof *sec_type); size_t *sec_size=calloc((size_t)total_sections,sizeof *sec_size); off_t *sec_off=calloc((size_t)total_sections,sizeof *sec_off);
    for(long s=0;s<total_sections;s++){ sec_type[s]=rd16(sb+s*6u,e); sec_size[s]=rd16(sb+s*6u+2,e); }
    free(sb); sec_off[0]=0; for(long s=1;s<total_sections;s++)sec_off[s]=sec_off[s-1]+(off_t)sec_size[s-1];
    if(sec_off[total_sections-1]+(off_t)sec_size[total_sections-1]>st.st_size){fprintf(stderr,"sections exceed file size\n");return 1;}

    int chosen=-1; for(unsigned i=0;i<nodes;i++)if(node[i].machine_type==UEEC_MACHINE_TYPE_CPU&&node[i].machine==HOST_MACHINE){chosen=(int)i;break;}
    if(chosen<0){fprintf(stderr,"no matching node\n");return 1;}

    int resident_g[32], resident_type[32], reloc_g[32], nr=0, nrel=0;
    for(long i=0;i<total_info;i++)if(info[i].n==chosen){ int s=info[i].s; if(s<0||s>=total_sections){fprintf(stderr,"bad mapping\n");return 1;} if(sec_type[s]==UEEC_SECTION_RELOCATION)reloc_g[nrel++]=s; else {resident_g[nr]=s;resident_type[nr]=sec_type[s];nr++;} }
    printf("node: %d, load-mode: %s\nresident sections: %d, temporary RELOC sections: %d\n",chosen,mode,nr,nrel);

    long ps=sysconf(_SC_PAGESIZE); if(ps<=0)ps=4096; size_t page=(size_t)ps;
    resident_t resident[32]={0}; void *local_base[32]={0}; size_t local_size[32]={0};
    void *mono=NULL; size_t mono_size=0;

    if(!strcmp(mode,"monolithic")){
        for(int i=0;i<nr;i++) mono_size += page_round(sec_size[resident_g[i]],page);
        mono=mmap(NULL,mono_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); if(mono==MAP_FAILED){perror("mmap resident");return 1;}
        size_t at=0; for(int i=0;i<nr;i++){ size_t m=page_round(sec_size[resident_g[i]],page); resident[i]=(resident_t){(uint8_t*)mono+at,m,resident_g[i],resident_type[i]}; local_base[i]=resident[i].addr; local_size[i]=sec_size[resident_g[i]]; if(pread_full(fd,resident[i].addr,local_size[i],sec_off[resident_g[i]])){perror("pread resident");return 1;} at+=m; }
    } else {
        struct iovec iov[32];
        for(int i=0;i<nr;i++){ size_t m=page_round(sec_size[resident_g[i]],page); void *a=mmap(NULL,m,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); if(a==MAP_FAILED){perror("mmap resident");return 1;} resident[i]=(resident_t){a,m,resident_g[i],resident_type[i]}; local_base[i]=a;local_size[i]=sec_size[resident_g[i]];iov[i].iov_base=a;iov[i].iov_len=local_size[i]; }
        /* preadv is used for runs that are physically contiguous. In this file DATA is
           separate and CODE is followed by RELOC, so each selected resident run is one iovec. */
        for(int i=0;i<nr;i++){ ssize_t got=preadv(fd,&iov[i],1,sec_off[resident_g[i]]); if(got!=(ssize_t)iov[i].iov_len){ if(got<0)perror("preadv resident");else fprintf(stderr,"short preadv\n");return 1;} }
    }

    for(int r=0;r<nrel;r++){
        size_t rs=sec_size[reloc_g[r]], rm=page_round(rs,page);
        temporary_t tmp={mmap(NULL,rm,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),rm,reloc_g[r]};
        if(tmp.addr==MAP_FAILED){perror("mmap RELOCATION");return 1;}
        if(pread_full(fd,tmp.addr,rs,sec_off[tmp.global_section])){perror("pread RELOCATION");return 1;}
        printf("RELOCATION section %d loaded temporarily (%zu bytes)\n",tmp.global_section,rs);
        if(process_reloc(tmp.addr,rs,e,local_base,local_size,nr))return 1;
        if(munmap(tmp.addr,tmp.map_size)<0){perror("munmap RELOCATION");return 1;}
        printf("RELOCATION section %d unmapped after relocation\n",tmp.global_section);
    }
    close(fd); /* file is no longer needed after relocation */

    void *entry=NULL;
    if(mono){
        /* One resident mapping cannot have per-section permissions without page-separated
           ranges; sections are page-separated above, so protect each range independently. */
        for(int i=0;i<nr;i++){ int prot=resident_type[i]==UEEC_SECTION_CODE?(PROT_READ|PROT_EXEC):PROT_READ; if(mprotect(resident[i].addr,resident[i].map_size,prot)<0){perror("mprotect");return 1;} if(resident_type[i]==UEEC_SECTION_CODE&&!entry)entry=resident[i].addr; }
    } else {
        for(int i=0;i<nr;i++){ int prot=resident_type[i]==UEEC_SECTION_CODE?(PROT_READ|PROT_EXEC):PROT_READ; if(mprotect(resident[i].addr,resident[i].map_size,prot)<0){perror("mprotect");return 1;} if(resident_type[i]==UEEC_SECTION_CODE&&!entry)entry=resident[i].addr; }
    }
    if(!entry){fprintf(stderr,"no CODE section found\n");return 1;}
    for(int i=0;i<nr;i++) if(resident_type[i]==UEEC_SECTION_CODE) { __builtin___clear_cache((char*)resident[i].addr,(char*)resident[i].addr+local_size[i]); break; }
    fflush(stdout);
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    volatile unsigned long fdesc[3]={(unsigned long)entry,0,0}; void(*fn)(void)=(void(*)(void))(unsigned long)fdesc; fn();
#elif defined(__powerpc64__)
    __asm__ volatile("mtctr %0\n\tbctr"::"r"(entry));
#else
    void(*fn)(void); memcpy(&fn,&entry,sizeof fn); fn();
#endif
    return 1;
}
