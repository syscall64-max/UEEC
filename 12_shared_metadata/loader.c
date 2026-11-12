/* Isolated stage-02 UEEC v1 parser copy with S390X detection; stages 00-09 are unchanged. */
#define main ueec_reference_main
#include "reference/ueec_load.c"
#undef main
#include "cpu.h"

#if !defined(__linux__)
#error This demonstration requires Linux
#endif

typedef struct {
    uint64_t required;
    uint16_t priority;
    size_t code_section;
    size_t metadata_section;
    unsigned metadata_fields;
    unsigned machine;
} isa_candidate;

static uint64_t md_integer(const uint8_t *p, size_t width, unsigned endian)
{
    if (width==2U) return read_u16(p,endian);
    if (width==4U) return read_u32(p,endian);
    return read_u64(p,endian);
}

static int parse_requirements(const uint8_t *p, size_t size, unsigned endian,
                              size_t section, size_t legacy_owner,
                              size_t nodes, isa_candidate *candidates)
{
    size_t width,entry_size,data_at,header_size;
    uint64_t count,previous=0;
    if (size<6U) return -1;
    unsigned design=read_u16(p+2,endian);
    if (design!=UEEC_MD_DESIGN_INDEXED && design!=UEEC_MD_DESIGN_NODE_INDEXED) return -1;
    if (design==UEEC_MD_DESIGN_INDEXED && legacy_owner==SIZE_MAX) return -1;
    unsigned format=read_u16(p,endian);
    if (format>2U) return -1;
    width=(size_t)2U<<format;
    size_t prefix=design==UEEC_MD_DESIGN_NODE_INDEXED ? 4U : 2U;
    entry_size=prefix+width;header_size=4U+width;
    if (!span_ok(size,4U,width)) return -1;
    count=md_integer(p+4,width,endian);
    /* Division bounds count before narrowing or adding the sentinel. */
    if (count==0U || count>=(uint64_t)((size-header_size)/entry_size)) return -1;
    data_at=header_size+((size_t)count+1U)*entry_size;
    for (size_t i=0;i<=(size_t)count;++i) {
        const uint8_t *element=p+header_size+i*entry_size;
        size_t node=design==UEEC_MD_DESIGN_NODE_INDEXED ? (size_t)read_u16(element,endian) : legacy_owner;
        uint16_t type=read_u16(element+prefix-2U,endian);
        uint64_t offset=md_integer(element+prefix,width,endian);
        if (offset>(uint64_t)(size-data_at) || offset<previous || (i==0U && offset!=0U)) return -1;
        if (i==(size_t)count) {
            if (type!=UEEC_MD_TYPE_END || offset!=(uint64_t)(size-data_at)) return -1;
            if (design==UEEC_MD_DESIGN_NODE_INDEXED && node!=0U) return -1;
        } else {
            if (node>=nodes || candidates[node].metadata_section!=section) return -1;
            isa_candidate *candidate=&candidates[node];
            uint16_t required_type=family_for(candidate->machine).metadata_type;
            uint64_t next=md_integer(element+entry_size+prefix,width,endian);
            if (next<offset || next>(uint64_t)(size-data_at)) return -1;
            if (required_type!=0U && type==required_type && !(candidate->metadata_fields&1U) && next-offset==8U) {
                candidate->required=read_u64(p+data_at+(size_t)offset,endian);
                candidate->metadata_fields|=1U;
            } else if (type==UEEC_MD_TYPE_PRIORITY && !(candidate->metadata_fields&2U) && next-offset==2U) {
                candidate->priority=read_u16(p+data_at+(size_t)offset,endian);
                candidate->metadata_fields|=2U;
            } else return -1; /* This prototype rejects unknown/duplicate records. */
        }
        previous=offset;
    }
    return 0;
}

static int collect_candidates(const file_view *file,const ueec_view *view,
                               isa_candidate *candidates)
{
    for (size_t node=0;node<view->nodes;++node) {
        candidates[node].machine=(node_word(file,view,node)>>10)&UINT32_C(0xff);
        candidates[node].code_section=SIZE_MAX;
        candidates[node].metadata_section=SIZE_MAX;
    }
    /* Discover section ownership before reading any metadata. Shared MD has
       one physical section and one existing UEEC mapping per owning node. */
    for (size_t i=0;i<view->node_info_count;++i) {
        size_t node=mapping_node(file,view,i);
        isa_candidate *candidate=&candidates[node];
        size_t global=mapping_section(file,view,i);
        section_view section;
        if (get_section(file,view,global,&section)!=0) return -1;
        if (section.type==UEEC_SECTION_CODE && candidate->code_section==SIZE_MAX && section.data_size>0U) {
            candidate->code_section=global;
        } else if (section.type==UEEC_SECTION_METADATA && candidate->metadata_section==SIZE_MAX) {
            candidate->metadata_section=global;
        } else return -1;
    }
    /* Validate every physical MD section once, including foreign-architecture
       and lower-priority nodes. Bad records must never be silently skipped. */
    for (size_t global=1;global<view->section_count;++global) {
        section_view section;uint64_t offset;
        if (get_section(file,view,global,&section)!=0) return -1;
        if (section.type!=UEEC_SECTION_METADATA) continue;
        size_t owner=SIZE_MAX,owners=0;
        for (size_t node=0;node<view->nodes;++node) {
            if (candidates[node].metadata_section==global) {owner=node;++owners;}
        }
        if (owners!=1U) owner=SIZE_MAX; /* legacy design 0 cannot be shared */
        if (owners==0U || section_file_offset(file,view,global,&offset)!=0 ||
            offset>(uint64_t)file->size || section.data_size>(uint64_t)file->size-offset ||
            parse_requirements(file->data+(size_t)offset,(size_t)section.data_size,
                               view->endian,global,owner,view->nodes,candidates)!=0) return -1;
    }
    for (size_t node=0;node<view->nodes;++node)
        if (candidates[node].code_section==SIZE_MAX ||
            candidates[node].metadata_section==SIZE_MAX ||
            !(candidates[node].metadata_fields&1U)) return -1;
    return 0;
}

static const char *technology_of(unsigned machine,uint64_t required)
{
    if (required==0U) return "scalar";
    if (machine==9U || machine==10U) {
        if (required & UEEC_X86_FLAG(UEEC_AVX512F)) return "avx512f";
        if (required & UEEC_X86_FLAG(UEEC_AVX2)) return "avx2";
        if (required & UEEC_X86_FLAG(UEEC_SSE2)) return "sse2";
    } else if (machine==1U) {
        if (required & 2U) return "sve";
        if (required & 1U) return "neon";
    } else if (machine==2U) {
        if (required & 2U) return "lasx";
        if (required & 1U) return "lsx";
    } else if (required & 1U) {
        switch (machine) {
        case 0: return "neon";
        case 3: case 4: return "altivec";
        case 5: case 6: return "rvv";
        case 7: case 8: return "vis";
        case 11: return "vx";
        default: break;
        }
    }
    return "custom";
}

static int technology_matches(unsigned machine,const char *technology,uint64_t required)
{
    if (!strcmp(technology,"avx512")) technology="avx512f";
    /* Exact profile matching: asking for neon must not silently choose SVE. */
    if (!strcmp(technology,"scalar") || !strcmp(technology,"sse2") ||
        !strcmp(technology,"avx2") || !strcmp(technology,"avx512f") ||
        !strcmp(technology,"neon") || !strcmp(technology,"sve") ||
        !strcmp(technology,"lsx") || !strcmp(technology,"lasx") ||
        !strcmp(technology,"altivec") || !strcmp(technology,"rvv") ||
        !strcmp(technology,"vis") || !strcmp(technology,"vx"))
        return !strcmp(technology,technology_of(machine,required));
    capability_family f=family_for(machine);
    for (unsigned i=0;i<f.count;++i)
        if (!strcmp(technology,f.names[i])) return (required&(UINT64_C(1)<<i))!=0U;
    return 0;
}

static int limited_mask(unsigned machine,uint64_t usable,const char *limit,uint64_t *out)
{
    *out=usable;
    if (!limit) return 0;
    if (!strcmp(limit,"scalar")) {*out=0;return 0;}
    uint64_t mask=0;
    if (machine==9U || machine==10U) {
        mask=UEEC_X86_FLAG(UEEC_SSE2);
        if (!strcmp(limit,"sse2")) {*out=usable&mask;return 0;}
        mask|=UEEC_X86_FLAG(UEEC_AVX)|UEEC_X86_FLAG(UEEC_AVX2);
        if (!strcmp(limit,"avx2")) {*out=usable&mask;return 0;}
        mask|=UEEC_X86_FLAG(UEEC_AVX512F);
        if (!strcmp(limit,"avx512f")) {*out=usable&mask;return 0;}
        return -1;
    }
    if ((machine==1U && !strcmp(limit,"sve")) ||
        (machine==2U && !strcmp(limit,"lasx"))) mask=3;
    else if (strcmp(limit,technology_of(machine,1))==0) mask=1;
    else return -1;
    *out=usable&mask;
    return 0;
}

static int execute_sum(int fd,const file_view *file,const ueec_view *view,
                        const isa_candidate *candidate)
{
    section_view code;uint64_t offset;size_t allocation;
    long page=sysconf(_SC_PAGESIZE);
    if (page<=0 || get_section(file,view,candidate->code_section,&code)!=0 ||
        section_file_offset(file,view,candidate->code_section,&offset)!=0 ||
        code.data_size>(uint64_t)SIZE_MAX ||
        align_up((size_t)code.data_size,(size_t)page,&allocation)!=0) return -1;
    void *entry=mmap(NULL,allocation,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (entry==MAP_FAILED) return -1;
    if (pread_full(fd,entry,(size_t)code.data_size,offset)!=0) {
        perror("read CODE");munmap(entry,allocation);return -1;
    }
    __builtin___clear_cache((char *)entry,(char *)entry+(size_t)code.data_size);
    if (mprotect(entry,allocation,PROT_READ|PROT_EXEC)!=0) {
        perror("load CODE");munmap(entry,allocation);return -1;
    }
    /* Release the whole-file inspection mapping before executing one CODE. */
    munmap((void *)file->data,file->size);close(fd);
    printf("EXECUTE resident_sections=1 metadata_resident=no\n");
    uint32_t (*function)(const uint32_t *,size_t);
    _Static_assert(sizeof(function)==sizeof(entry),"function pointer size");
    #if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF==1)
    /* ELFv1 function pointers address descriptors, not machine instructions.
       This leaf uses no TOC/global state; environment and TOC are zero. */
    uintptr_t descriptor[3]={(uintptr_t)entry,0,0};
    /* The call sequence reads this object through a function pointer. Those
       ABI-level reads are invisible to C dead-store analysis (GCC 15 removes
       the initializer without this explicit memory operand). */
    __asm__ volatile ("" : : "m"(descriptor) : "memory");
    void *descriptor_address=descriptor;
    memcpy(&function,&descriptor_address,sizeof(function));
#else
    memcpy(&function,&entry,sizeof(function));
#endif
    _Alignas(64) uint32_t data[1040];uint32_t state=UINT32_C(0x12345678);
    for (size_t i=0;i<1040U;++i) { state=state*1664525U+1013904223U;data[i]=state; }
    const size_t lengths[]={0,1,3,4,7,8,15,16,17,31,32,63,64,65,257,1024};
    int rc=0;
    for (size_t alignment=0;alignment<16U;++alignment) {
      for (size_t i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i) {
        uint32_t expected=0;size_t n=lengths[i];
        for (size_t j=0;j<n;++j) expected+=data[j+alignment];
        uint32_t got=function(data+alignment,n);
        if (got!=expected) {
            fprintf(stderr,"FAIL: length=%zu got=%" PRIu32 " expected=%" PRIu32 "\n",n,got,expected);rc=-1;break;
        }
      }
    }
    /* A guard page on each side catches reads outside the requested array.
       Exercise both ends and every tested length, including zero. */
    size_t page_size=(size_t)page;
    uint8_t *guard=mmap(NULL,page_size*3U,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (guard==MAP_FAILED || mprotect(guard+page_size,page_size,PROT_READ|PROT_WRITE)!=0) {
        perror("guard pages");munmap(entry,allocation);return -1;
    }
    for (size_t i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i) {
        size_t n=lengths[i];
        if (n>page_size/sizeof(uint32_t)) {rc=-1;break;}
        for (unsigned side=0;side<2U;++side) {
            uint32_t *p=(uint32_t *)(guard+page_size+(side ? page_size-n*4U : 0U));
            uint32_t expected=0;
            for (size_t j=0;j<n;++j) {p[j]=UINT32_MAX-(uint32_t)j;expected+=p[j];}
            if (function(p,n)!=expected) {fprintf(stderr,"FAIL: guard page length=%zu\n",n);rc=-1;}
        }
    }
    if (function(NULL,0)!=0U) rc=-1;
    munmap(guard,page_size*3U);
    for (size_t i=0;i<64U;++i) data[i]=(uint32_t)i+1U;
    uint32_t result=function(data,64);
    if (result!=2080U) rc=-1;
    printf("sum(1..64) = %" PRIu32 "; expected=2080\n",result);
    if (rc==0) printf("PASS: 256 alignment/length cases, 32 guard-page cases, NULL/zero, tails and uint32 overflow\n");
    munmap(entry,allocation);return rc;
}

static void dispatch_usage(const char *program)
{
    printf("Usage: %s [options] FILE.ueec\n"
           "  --technology NAME     request technology: scalar/neon/sve/lsx/lasx/altivec/rvv/vis/vx/sse2/avx2/avx512f\n"
           "  --fallback            use automatic selection if request fails\n"
           "  --list-technologies   list every architecture and variant; do not execute\n"
           "  --host-only           restrict listing to the current architecture\n"
           "  --cpu                 display CPU and OS-usable features\n"
           "  --groups              display known flags for this architecture\n"
           "  --no-run              select and report without executing\n"
           "  --limit-isa PROFILE   test only: remove capabilities, never add them\n"
           "  --help                display this help\n"
           "No --technology: choose highest compatible METADATA priority.\n",program);
}

int main(int argc,char **argv)
{
    const char *path=NULL,*technology=NULL,*limit=NULL;
    int fallback=0,list=0,show_cpu=0,groups=0,no_run=0,host_only=0;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--help")) {dispatch_usage(argv[0]);return 0;}
        else if (!strcmp(argv[i],"--technology") && i+1<argc) technology=argv[++i];
        else if (!strncmp(argv[i],"--technology=",13)) technology=argv[i]+13;
        else if (!strcmp(argv[i],"--limit-isa") && i+1<argc) limit=argv[++i];
        else if (!strcmp(argv[i],"--fallback")) fallback=1;
        else if (!strcmp(argv[i],"--list-technologies")) list=1;
        else if (!strcmp(argv[i],"--cpu")) show_cpu=1;
        else if (!strcmp(argv[i],"--groups")) groups=1;
        else if (!strcmp(argv[i],"--host-only")) host_only=1;
        else if (!strcmp(argv[i],"--no-run")) no_run=1;
        else if (argv[i][0]=='-' || path) {dispatch_usage(argv[0]);return 2;}
        else path=argv[i];
    }
    if (technology && !strcmp(technology,"auto")) technology=NULL;
    if (fallback && !technology) {dispatch_usage(argv[0]);return 2;}
    dispatch_cpu cpu=dispatch_detect_cpu();
    capability_family host_family=family_for(cpu.machine);
    uint64_t usable;
    if (!host_family.count || limited_mask(cpu.machine,cpu.usable,limit,&usable)!=0) {
        fprintf(stderr,"error: unsupported host architecture or --limit-isa profile\n");return 2;
    }
    if (show_cpu || groups || path) {
        printf("CPU architecture: %s | %s\n",host_family.architecture,cpu.description);
        printf("Detection: %s\n",cpu.source);
        printf("OS usable: ");print_capabilities(cpu.machine,cpu.usable);putchar('\n');
        if (limit) {printf("Test limit: %s; effective: ",limit);print_capabilities(cpu.machine,usable);putchar('\n');}
    }
    if (groups) {
        printf("bit feature          hardware usable\n");
        for (unsigned i=0;i<host_family.count;++i)
            printf("%2u  %-16s %-8s %s\n",i,host_family.names[i],
                cpu.hardware_known ? (cpu.hardware&(UINT64_C(1)<<i) ? "yes" : "no") : "n/a",
                cpu.usable&(UINT64_C(1)<<i) ? "yes" : "no");
        if (!cpu.hardware_known) printf("hardware=n/a: Linux auxv reports OS availability, not a separate hardware probe.\n");
    }
    if (!path) {
        if ((show_cpu || groups) && !technology && !list) return 0;
        dispatch_usage(argv[0]);return 2;
    }
    int fd=open(path,O_RDONLY);
    struct stat st;
    if (fd<0) {perror(path);return 2;}
    if (fstat(fd,&st)!=0 || st.st_size<=0 || (uintmax_t)st.st_size>64U*1024U*1024U) {
        fprintf(stderr,"error: invalid file size\n");close(fd);return 1;
    }
    void *mapping=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if (mapping==MAP_FAILED) {perror("mmap");close(fd);return 1;}
    file_view file={mapping,(size_t)st.st_size};ueec_view view;
    isa_candidate *candidates=NULL;size_t best=SIZE_MAX,requested=SIZE_MAX;
    int present=0,rc=1;
    if (parse_ueec(&file,&view)!=0) goto cleanup;
    candidates=calloc(view.nodes,sizeof(*candidates));
    if (!candidates) goto cleanup;
    if (collect_candidates(&file,&view,candidates)!=0) {
        fprintf(stderr,"error: malformed CODE/METADATA\n");goto cleanup;
    }
    for (size_t i=0;i<view.nodes;++i) {
        uint32_t word=node_word(&file,&view,i);
        unsigned machine=(word>>10)&UINT32_C(0xff);
        if (((word>>7)&7U)!=UEEC_MACHINE_TYPE_CPU ||
            ((word>>18)&3U)!=UEEC_SYSTEM_TYPE_GENERAL_OS ||
            ((word>>20)&127U)!=UEEC_SYSTEM_LINUX) continue;
        /* This demo calls a user-space leaf library with a known ABI. */
        int abi=(word&UINT32_C(0x1e))==UINT32_C(0x14);
        int host=node_matches_host(&file,&view,i) && abi &&
                 (word&1U)==UEEC_HOST_ENDIAN;
        (void)machine;
        if ((!list && !host) || (host_only && !host) || !node_name_equals(&file,&view,i,"sum")) continue;
        uint64_t required=candidates[i].required;
        int compatible=host && (required & ~usable)==0U;
        if (list) {
            printf("node=%zu architecture=%s technology=%s CODE=%zu METADATA=%zu priority=%u compatible=%s requires=",
                   i,family_for(candidates[i].machine).architecture,technology_of(candidates[i].machine,required),candidates[i].code_section,
                   candidates[i].metadata_section,
                   (unsigned)candidates[i].priority,compatible ? "yes" : "no");
            print_capabilities(candidates[i].machine,required);
            putchar('\n');
        }
        if (compatible && (best==SIZE_MAX || candidates[i].priority>candidates[best].priority)) best=i;
        if (host && technology && technology_matches(cpu.machine,technology,required)) {
            present=1;
            if (compatible && (requested==SIZE_MAX || candidates[i].priority>candidates[requested].priority)) requested=i;
        }
    }
    if (list) {rc=0;goto cleanup;}
    if (technology && requested==SIZE_MAX) {
        fprintf(stderr,"%s: requested technology '%s' %s\n",fallback ? "FALLBACK" : "error",
                technology,present ? "cannot run with current CPU/OS capabilities" : "is not present in this file");
        if (!fallback) goto cleanup;
    }
    size_t selected=requested!=SIZE_MAX ? requested : best;
    if (selected==SIZE_MAX) {fprintf(stderr,"error: no compatible node\n");goto cleanup;}
    printf("SELECTED architecture=%s technology=%s node=%zu CODE=%zu priority=%u mode=%s\n",
        host_family.architecture,technology_of(cpu.machine,candidates[selected].required),selected,candidates[selected].code_section,
        (unsigned)candidates[selected].priority,requested!=SIZE_MAX ? "requested" : "automatic");
    fflush(stdout);
    if (no_run) {rc=0;goto cleanup;}
    isa_candidate chosen=candidates[selected];free(candidates);candidates=NULL;
    /* execute_sum closes fd and unmaps file after successful CODE loading. */
    rc=execute_sum(fd,&file,&view,&chosen);
    if (rc==-1) {
        /* Loading errors may precede cleanup; process exit releases remaining resources. */
        return 1;
    }
    return 0;
cleanup:
    free(candidates);munmap(mapping,file.size);close(fd);return rc;
}
