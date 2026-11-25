#ifndef UEEC_DISPATCH_COMMON_H
#define UEEC_DISPATCH_COMMON_H
/* Reuse the exact stage-02 UEEC v1 parser, without changing stages 00-10. */
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

static inline uint64_t md_integer(const uint8_t *p, size_t width, unsigned endian)
{
    if (width==2U) return read_u16(p,endian);
    if (width==4U) return read_u32(p,endian);
    return read_u64(p,endian);
}

static inline int parse_requirements(const uint8_t *p, size_t size, unsigned endian,
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

static inline int collect_candidates(const file_view *file,const ueec_view *view,
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

static inline const char *technology_of(unsigned machine,uint64_t required)
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

static inline int technology_matches(unsigned machine,const char *technology,uint64_t required)
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

static inline int limited_mask(unsigned machine,uint64_t usable,const char *limit,uint64_t *out)
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


#endif
