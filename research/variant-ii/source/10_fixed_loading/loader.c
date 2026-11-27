#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include "import.h"
#include "metadata.h"
#include "../09_convert_export/ueec_export.h"

/* Internal/external import experiment. Numeric wire fields are decoded
 * explicitly; resident bytes always have the target machine's byte order. */
typedef struct { const uint8_t *p; size_t n; unsigned be; } view;
typedef struct { unsigned type; size_t size, data_size; view raw; void *mem; } section;
typedef struct { uint8_t *file; size_t file_size; unsigned owner; void *arena[2]; size_t arena_size[2]; void *mapped[2016]; section s[32]; size_t count, resident[32], nr; unsigned be; view exports, imports; } image;
static image images[65];
static size_t ni;
static enum { SCATTERED, MONOLITHIC, HYBRID } mode=SCATTERED;
static size_t page, mappings;
static int fixed_loading;
#ifdef UEEC_TEST_HOOKS
static void *occupied;
#endif
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
static uintptr_t descriptors[4097][3];
static size_t nd;
#endif
static void cleanup(void) {
#ifdef UEEC_TEST_HOOKS
    if(occupied) munmap(occupied,page);
#endif
    for (size_t j=0;j<ni;j++) {
        for (size_t i=0;i<images[j].count;i++) if(mode==SCATTERED && images[j].s[i].mem) munmap(images[j].s[i].mem,images[j].s[i].size);
        for(size_t k=0;k<2;k++) if(images[j].arena[k]) munmap(images[j].arena[k],images[j].arena_size[k]);
        if(images[j].owner) free(images[j].file);
    }
}
static void fail(const char *s) { fprintf(stderr,"IMPORT error: %s\n",s); exit(1); }
static size_t mul(size_t a,size_t b) { if(b && a>SIZE_MAX/b) fail("size overflow"); return a*b; }
static size_t sz(uint64_t x) { if(x>SIZE_MAX) fail("size overflow"); return (size_t)x; }
static view slice(view v,size_t o,size_t n) { if(o>v.n || n>v.n-o) fail("truncated field"); return (view){v.p+o,n,v.be}; }
static view take(view *v,size_t n) { view r=slice(*v,0,n); v->p+=n; v->n-=n; return r; }
static uint64_t readnum(view v,size_t o,unsigned w) {
    view a=slice(v,o,w); uint64_t r=0;
    for(unsigned i=0;i<w;i++) r=(r<<8)|a.p[v.be?i:w-1-i];
    return r;
}
static uint64_t get(view *v,unsigned w) { view a=take(v,w); return readnum(a,0,w); }
static unsigned width(uint64_t f) { if(f>2) fail("unsupported format"); return 2u<<(unsigned)f; }
static const char *name_at(view v,uint64_t off) {
    size_t o=sz(off); if(o>=v.n || !v.p[o] || (o && v.p[o-1]) || !memchr(v.p+o,0,v.n-o)) fail("invalid name offset or terminator");
    return (const char*)v.p+o;
}
static int host_machine(void)
{
#if defined(__arm__)
    return UEEC_MACHINE_ARM32;
#elif defined(__aarch64__)
    return UEEC_MACHINE_ARM64;
#elif defined(__loongarch64) || defined(__loongarch__)
    return UEEC_MACHINE_LOONGARCH64;
#elif defined(__powerpc64__)
    return UEEC_MACHINE_POWERPC64;
#elif defined(__powerpc__)
    return UEEC_MACHINE_POWERPC32;
#elif defined(__riscv) && (__riscv_xlen == 32)
    return UEEC_MACHINE_RISCV32;
#elif defined(__riscv) && (__riscv_xlen == 64)
    return UEEC_MACHINE_RISCV64;
#elif defined(__sparc__) && defined(__arch64__)
    return UEEC_MACHINE_SPARC64;
#elif defined(__sparc__)
    return UEEC_MACHINE_SPARC32;
#elif defined(__s390x__)
    return UEEC_MACHINE_S390X;
#elif defined(__i386__)
    return UEEC_MACHINE_X86;
#elif defined(__x86_64__)
    return UEEC_MACHINE_X86_64;
#else
    return -1;
#endif
}
/* A contiguous, page-rounded runtime layout per physical container.
 * Hybrid keeps the header in a separate mapping. No file-backed mmap yet. */
static size_t rounded(size_t n) {
    if(n>SIZE_MAX-(page-1)) fail("page rounding overflow");
    return ((n+page-1)/page)*page;
}
static void *alloc_region(size_t n) {
    void *p=mmap(NULL,n,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==MAP_FAILED) fail("mmap");
    mappings++; return p;
}
static void map_container(image *im,section *physical,size_t max, uintptr_t base) {
    size_t totals[2]={0,0}, offsets[2016]={0};
    for(size_t i=0;i<=max;i++) {
        size_t k=mode==HYBRID && i!=0, n=rounded(physical[i].size);
        if(n>SIZE_MAX-totals[k]) fail("arena size overflow");
        offsets[i]=totals[k]; totals[k]+=n;
    }
    for(size_t k=0;k<2;k++) if(totals[k]) {
        if(base) {
            if(totals[k]>UINTPTR_MAX-base) fail("fixed range overflow");
            #ifdef UEEC_TEST_HOOKS
            if(getenv("UEEC_TEST_OCCUPIED")) {
                void *q=mmap((void*)base,page,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
                if(q==MAP_FAILED || (uintptr_t)q!=base) fail("test reservation unavailable");
                occupied=q; memset(q,0x5a,page);
            }
#endif
            void *p=mmap((void*)base,totals[k],PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
            if(p==MAP_FAILED) {
#ifdef UEEC_TEST_HOOKS
                if(occupied) { for(size_t i=0;i<page;i++) if(((unsigned char*)occupied)[i]!=0x5a) fail("test reservation modified"); fprintf(stderr,"OCCUPIED_UNCHANGED\n"); }
#endif
                fail("fixed mapping unavailable");
            }
            if((uintptr_t)p!=base) { munmap(p,totals[k]); fail("fixed mapping unsupported"); }
            im->arena[k]=p; mappings++;
        } else im->arena[k]=alloc_region(totals[k]);
        im->arena_size[k]=totals[k];
    }
    for(size_t i=0;i<=max;i++) {
        section *s=&physical[i]; size_t k=mode==HYBRID && i!=0;
        if(!s->size) continue;
        void *p=(uint8_t*)im->arena[k]+offsets[i]; im->mapped[i]=p;
        memcpy(p,s->raw.p,s->data_size);
        int prot=PROT_READ;
        if(s->type==UEEC_SECTION_CODE) {
            __builtin___clear_cache(p,(char*)p+s->size); prot|=PROT_EXEC;
        } else if(s->type==UEEC_SECTION_INIT_DATA || s->type==UEEC_SECTION_UNINIT_DATA) prot|=PROT_WRITE;
        if(mprotect(p,rounded(s->size),prot)) fail("mprotect arena section");
    }
}
static image *load_image(const char *path,int lib,const char *wanted,image *source) {
    if(ni>=sizeof(images)/sizeof(images[0])) fail("too many images (experimental limit)");
    image *im=&images[ni++];
    if(source) { im->file=source->file; im->file_size=source->file_size; } else {
    FILE *f=fopen(path,"rb"); if(!f) { fprintf(stderr,"cannot open %s\n",path); fail("missing library or program"); }
    if(fseek(f,0,SEEK_END)) { fclose(f); fail("seek"); }
    long n=ftell(f); if(n<10 || n>64*1024*1024 || fseek(f,0,SEEK_SET)) { fclose(f); fail("invalid file size"); }
    im->file=malloc((size_t)n); im->owner=1; if(!im->file) { fclose(f); fail("allocation"); }
    size_t got=fread(im->file,1,(size_t)n,f); fclose(f); if(got!=(size_t)n) fail("read");
    im->owner=1; im->file_size=(size_t)n;
    }
    size_t n=im->file_size; uint8_t *b=im->file;
    if(memcmp(b,"_GM",3) || (b[3]&127)!=UEEC_VERSION_2) fail("bad magic/version");
    im->be=b[3]>>7; view whole={b,(size_t)n,im->be}, v=slice(whole,4,(size_t)n-4);
    unsigned w=width(get(&v,2)); size_t nodes=sz(get(&v,2)), names=sz(get(&v,2));
    if(!nodes || nodes>65 || !names || names>nodes) fail("invalid node/name count");
    uint32_t words[65]; size_t links=0, nc[65]={0};
    for(size_t i=0;i<nodes;i++) { words[i]=(uint32_t)get(&v,4); links+=words[i]>>27; }
    size_t order[65][31], max=0; unsigned seen[2016]={0};
    for(size_t i=0;i<links;i++) {
        size_t ndx=sz(get(&v,2)), sec=sz(get(&v,4));
        if(ndx>=nodes || !sec || sec>links || nc[ndx]>=(words[ndx]>>27) || seen[sec]++) fail("invalid or shared section index (stage 10.2)");
        order[ndx][nc[ndx]++]=sec; if(sec>max) max=sec;
    }
    if(max!=links) fail("sparse section index");
    section physical[2016]={0}; size_t total=0;
    for(size_t i=0;i<=max;i++) {
        section *s=&physical[i]; s->type=(unsigned)get(&v,2); s->size=sz(get(&v,w)); s->data_size=sz(get(&v,w));
        if(s->data_size>s->size || total>whole.n || s->size>whole.n-total) fail("section bounds");
        s->raw=slice(whole,total,s->data_size); total+=s->size;
    }
    view lengths=take(&v,names), indexes=take(&v,2*nodes); view strings[65];
    for(size_t i=0;i<names;i++) { size_t len=lengths.p[i]; if(!len) fail("invalid node name"); strings[i]=take(&v,len); }
    if(physical[0].type!=0 || physical[0].data_size!=(size_t)(v.p-b) || total!=whole.n) fail("header/file bounds");
    size_t chosen=SIZE_MAX;
    for(size_t i=0;i<nodes;i++) {
        size_t idx=sz(readnum(indexes,i*2,2)); if(idx>=names || nc[i]!=(words[i]>>27)) fail("invalid node mapping/name");
        uint32_t node=words[i];
        if(((node>>10)&255)!=(unsigned)host_machine() || (node&1)!=UEEC_HOST_ENDIAN || ((node>>7)&7)!=0 || ((node>>18)&3)!=2 || ((node>>20)&127)!=2 || !(node&(1u<<(lib?4:3))) || !(node&4)) continue;
        if(wanted && (strlen(wanted)!=strings[idx].n || memcmp(wanted,strings[idx].p,strings[idx].n))) continue;
        if(chosen!=SIZE_MAX) fail("ambiguous matching node");
        chosen=i;
    }
    if(chosen==SIZE_MAX) fail("missing compatible node");
    uintptr_t base=0;
    if(!source) {
        view meta={0};
        for(size_t j=1;j<=max;j++) if(physical[j].type==UEEC_SECTION_METADATA) {
            if(meta.p) fail("duplicate metadata");
            meta=physical[j].raw;
        }
        if(meta.p) {
            if(lib || mode!=MONOLITHIC) fail("fixed metadata requires monolithic program");
            if(get(&meta,2)!=UEEC_METADATA_LOAD_INFO) fail("unknown metadata type");
            unsigned aw=width(get(&meta,2));
            if(get(&meta,2)!=nodes) fail("metadata node count");
            size_t offsets[2016]={0}, end=0;
            for(size_t j=0;j<=max;j++) { offsets[j]=end; size_t z=rounded(physical[j].size); if(z>SIZE_MAX-end) fail("layout overflow"); end+=z; }
            unsigned used[65]={0};
            for(size_t j=0;j<nodes;j++) {
                size_t idx=sz(get(&meta,2)); uint64_t addr=get(&meta,aw);
                if(idx>=nodes || used[idx]++ || !nc[idx] || addr>UINTPTR_MAX || addr%page) fail("invalid fixed node address");
                size_t off=offsets[order[idx][0]];
                if(addr<=off) fail("invalid fixed base");
                uintptr_t candidate=(uintptr_t)addr-off;
                if(base && base!=candidate) fail("inconsistent fixed layout");
                base=candidate;
                
            }
            if(meta.n || !base || base%page || end>UINTPTR_MAX-base) fail("invalid fixed metadata bounds");
            fixed_loading=1;
        } else if(fixed_loading) fail("external library in fixed experiment");
    }
    if(mode!=SCATTERED && !source) map_container(im,physical,max,base);
    if(base) for(size_t j=0;j<nodes;j++) printf("FIXED_NODE=%zu ADDRESS=0x%llx\n",j,(unsigned long long)(uintptr_t)im->mapped[order[j][0]]);
    image *backing=source?source:im;
    im->count=nc[chosen]+1; if(im->count==1) fail("empty node");
    for(size_t i=1;i<im->count;i++) {
        im->s[i]=physical[order[chosen][i-1]];
        section *s=&im->s[i];
        if(mode!=SCATTERED && s->size) s->raw.p=backing->mapped[order[chosen][i-1]];
        switch(s->type) {
        case UEEC_SECTION_CODE: case UEEC_SECTION_INIT_DATA: case UEEC_SECTION_UNINIT_DATA: case UEEC_SECTION_CONSTANT_DATA:
            if(!s->size) fail("empty resident section");
            if(mode==SCATTERED) {
                s->mem=alloc_region(s->size);
                memcpy(s->mem,s->raw.p,s->data_size);
                if(s->type==UEEC_SECTION_CODE) {
                    __builtin___clear_cache(s->mem,(char*)s->mem+s->size);
                    if(mprotect(s->mem,s->size,PROT_READ|PROT_EXEC)) fail("mprotect code");
                } else if(s->type==UEEC_SECTION_CONSTANT_DATA && mprotect(s->mem,s->size,PROT_READ)) fail("mprotect constant");
            } else s->mem=backing->mapped[order[chosen][i-1]];
            im->resident[im->nr++]=i;
            break;
        case UEEC_SECTION_METADATA: break;
        case UEEC_SECTION_EXPORT: if(im->exports.p) fail("duplicate export"); im->exports=s->raw; break;
        case UEEC_SECTION_IMPORT: if(im->imports.p) fail("duplicate import"); im->imports=s->raw; break;
        default: fail("unsupported section in first experiment");
        }
    }
    if(lib && (!im->exports.p || im->imports.p)) fail("library must export and have no dependencies in stage 10.1");
    if(!lib && !im->imports.p) fail("program has no import section");
    return im;
}
static uintptr_t callable(void *p) {
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
    if(nd>=sizeof(descriptors)/sizeof(descriptors[0])) fail("descriptor limit");
    descriptors[nd][0]=(uintptr_t)p; descriptors[nd][1]=0; descriptors[nd][2]=0;
    return (uintptr_t)&descriptors[nd++][0];
#else
    return (uintptr_t)p;
#endif
}
static uintptr_t address(image *im,uint64_t sec,uint64_t off) {
    if(sec>=im->nr) fail("invalid export section");
    section *s=&im->s[im->resident[sec]];
    if(off>=s->size) fail("invalid export offset");
    void *p=(uint8_t*)s->mem+(size_t)off;
    return s->type==UEEC_SECTION_CODE?callable(p):(uintptr_t)p;
}
/* Existing EXPORT wire layouts, with binary search on sorted symbol names. */
static uintptr_t resolve(image *im,const char *name) {
    view v=im->exports; unsigned w=width(get(&v,2)); uint64_t design=get(&v,2);
    unsigned cw=width(get(&v,2)); size_t n=sz(get(&v,cw)), len=strlen(name);
    if(design==UEEC_EXPORT_DESIGN_COMPACT_GROUP) {
        size_t gw=2+cw, iw=1+w; view groups=take(&v,mul(n,gw));
        for(size_t g=0;g<n;g++) {
            size_t gl=sz(readnum(groups,g*gw,2)), count=sz(readnum(groups,g*gw+2,cw));
            if(!gl) fail("zero export name length");
            view info=take(&v,mul(count,iw)), names=take(&v,mul(count,gl));
            if(gl!=len) continue;
            size_t lo=0,hi=count;
            while(lo<hi) {
                size_t m=lo+(hi-lo)/2; int cmp=memcmp(name,names.p+m*gl,gl);
                if(!cmp) return address(im,readnum(info,m*iw,1)&31,readnum(info,m*iw+1,w));
                if(cmp<0) hi=m; else lo=m+1;
            }
        }
    } else if(design==UEEC_EXPORT_DESIGN_SIMPLE_FLAT) {
        size_t iw=2+2*w; view info=take(&v,mul(n,iw)); size_t lo=0,hi=n;
        while(lo<hi) {
            size_t m=lo+(hi-lo)/2, p=m*iw; unsigned bits=(unsigned)readnum(info,p,2); size_t sl=bits>>5;
            view str=slice(v,sz(readnum(info,p+2+w,w)),sl);
            size_t common=len<sl?len:sl; int cmp=memcmp(name,str.p,common);
            if(!cmp) cmp=(len>sl)-(len<sl);
            if(!cmp) return address(im,bits&31,readnum(info,p+2,w));
            if(cmp<0) hi=m; else lo=m+1;
        }
    } else fail("unsupported export design (named import only)");
    fprintf(stderr,"symbol not found: %s\n",name); fail("missing symbol"); return 0;
}
static void bind_imports(image *im,const char *program_path) {
    view v=im->imports; if(get(&v,2)!=UEEC_IMPORT_DESIGN_COMPACT_GROUP) fail("unsupported import design");
    unsigned w=width(get(&v,2)); size_t n=sz(get(&v,w)); if(n>64) fail("library limit");
    view libs=take(&v,mul(n,w)), counts=take(&v,mul(n,w)); size_t m=0;
    for(size_t i=0;i<n;i++) { size_t k=sz(readnum(counts,i*w,w)); if(k>SIZE_MAX-m) fail("object count overflow"); m+=k; }
    if(m>4096) fail("object limit");
    view objects=take(&v,mul(m,3*w)), names=v;
    uint64_t external=UINT64_C(1)<<(8*w-1); size_t obj=0;
    const char *slash=strrchr(program_path,'/'); size_t dir=slash?(size_t)(slash-program_path+1):0;
    for(size_t i=0;i<n;i++) {
        uint64_t entry=readnum(libs,i*w,w); 
        const char *lib=name_at(names,entry&~external);
        if(strchr(lib,'/') || strchr(lib,'\\') || !strcmp(lib,".") || !strcmp(lib,"..")) fail("library must be a filename");
        char path[4096]; size_t l=strlen(lib); if(dir>=sizeof(path) || l>=sizeof(path)-dir) fail("library path too long");
        memcpy(path,program_path,dir); memcpy(path+dir,lib,l+1);
        if(fixed_loading && (entry&external)) fail("external library in fixed experiment");
        image *library=(entry&external)?load_image(path,1,NULL,NULL):load_image(program_path,1,lib,im); size_t count=sz(readnum(counts,i*w,w));
        for(size_t j=0;j<count;j++,obj++) {
            size_t p=obj*3*w; uint64_t sec=readnum(objects,p,w), off=readnum(objects,p+w,w);
            const char *symbol=name_at(names,readnum(objects,p+2*w,w));
            if(sec>=im->nr) fail("invalid import target section");
            section *s=&im->s[im->resident[sec]];
            if(s->type!=UEEC_SECTION_INIT_DATA && s->type!=UEEC_SECTION_UNINIT_DATA) fail("import target not writable data");
            if(off>s->size || sizeof(uintptr_t)>s->size-(size_t)off || off%sizeof(uintptr_t)) fail("invalid import target offset");
            uintptr_t ptr=resolve(library,symbol); memcpy((uint8_t*)s->mem+(size_t)off,&ptr,sizeof(ptr));
            printf("BOUND %s:%s -> section=%llu offset=%llu\n",lib,symbol,(unsigned long long)sec,(unsigned long long)off);
        }
    }
    printf("IMPORT format=%u libraries=%zu objects=%zu\n",w*8,n,m);
}
int main(int argc,char **argv) {
    const char *path=NULL;
    if(argc==2 && !strcmp(argv[1],"--page-size")) { long ps=sysconf(_SC_PAGESIZE); if(ps<=0) fail("page size"); printf("%ld\n",ps); return 0; }
    if(argc==2) path=argv[1];
    else if(argc==4 && !strcmp(argv[1],"--load-mode")) {
        if(!strcmp(argv[2],"monolithic")) mode=MONOLITHIC;
        else if(!strcmp(argv[2],"hybrid")) mode=HYBRID;
        else if(strcmp(argv[2],"scattered")) fail("unknown load mode");
        path=argv[3];
    } else fail("usage: loader [--load-mode monolithic|scattered|hybrid] program.ueec");
    long ps=sysconf(_SC_PAGESIZE); if(ps<=0) fail("page size"); page=(size_t)ps;
    atexit(cleanup); image *im=load_image(path,0,NULL,NULL); bind_imports(im,path);
    /* Stage 10.1 fixture entry ABI: int entry(const struct slots *).
     * The DATA section contains exactly the four imported addresses. */
    if(im->nr!=2 || im->s[im->resident[0]].type!=UEEC_SECTION_CODE || im->s[im->resident[1]].type!=UEEC_SECTION_INIT_DATA || im->s[im->resident[1]].size<4*sizeof(uintptr_t)) fail("unsupported test entry ABI");
    section *data=&im->s[im->resident[1]];
    for(size_t i=0;i<4;i++) { uintptr_t p; memcpy(&p,(uint8_t*)data->mem+i*sizeof(p),sizeof(p)); if(!p) fail("unbound entry slot"); }
    int (*entry)(const void*)=(int (*)(const void*))callable(im->s[im->resident[0]].mem);
    int result=entry(data->mem); printf("LOAD_MODE=%s MAPPINGS=%zu\n",mode==MONOLITHIC?"monolithic":mode==HYBRID?"hybrid":"scattered",mappings); printf("PROGRAM_RESULT=%d\n",result); return 0;
}
