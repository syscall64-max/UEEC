#ifndef UEEC_NET_SELECTION_H
#define UEEC_NET_SELECTION_H

#include "../dispatch_common.h"
#include "ueec_wire.h"

#define UEEC_NET_MAGIC "UEC2"
#define UEEC_REQUEST_SIZE 64U
#define UEEC_RESPONSE_SIZE 64U
#define UEEC_TECH_SIZE 32U

enum net_status { NET_OK, NET_BAD_REQUEST, NET_NO_MATCH, NET_TECH_UNAVAILABLE, NET_INTERNAL };

typedef struct {
    unsigned machine, system, endian;
    int fallback;
    uint64_t usable;
    char technology[UEEC_TECH_SIZE];
} net_request;

static inline int net_technology_valid(const char *s)
{
    for (size_t i=0;i<UEEC_TECH_SIZE;++i) {
        unsigned char c=(unsigned char)s[i];
        if (!c) return 1;
        if (!((c>='a' && c<='z') || (c>='0' && c<='9') || c=='_')) return 0;
    }
    return 0;
}

static inline int net_request_encode(const net_request *r,uint8_t p[UEEC_REQUEST_SIZE])
{
    if (r->machine>255U || !family_for(r->machine).count || r->system>127U ||
        r->endian>1U || !net_technology_valid(r->technology) ||
        (r->fallback && !r->technology[0])) return -1;
    memset(p,0,UEEC_REQUEST_SIZE);memcpy(p,UEEC_NET_MAGIC,4);
    p[4]=(uint8_t)r->machine;p[5]=(uint8_t)r->system;p[6]=(uint8_t)r->endian;
    p[7]=(uint8_t)(r->fallback ? 1U : 0U);
    write_u64(p+8,r->usable,UEEC_ENDIAN_BIG);
    memcpy(p+16,r->technology,strlen(r->technology));
    return 0;
}

static inline int net_request_decode(const uint8_t p[UEEC_REQUEST_SIZE],net_request *r)
{
    if (memcmp(p,UEEC_NET_MAGIC,4) || p[6]>1U || p[7]>1U) return -1;
    for (size_t i=48;i<UEEC_REQUEST_SIZE;++i) if (p[i]) return -1;
    memset(r,0,sizeof(*r));r->machine=p[4];r->system=p[5];r->endian=p[6];
    r->fallback=p[7]!=0U;r->usable=read_u64(p+8,UEEC_ENDIAN_BIG);
    memcpy(r->technology,p+16,UEEC_TECH_SIZE);
    if (!memchr(r->technology,0,UEEC_TECH_SIZE)) return -1;
    size_t len=strlen(r->technology);
    for (size_t i=len;i<UEEC_TECH_SIZE;++i) if (r->technology[i]) return -1;
    if (!family_for(r->machine).count || r->system>127U ||
        !net_technology_valid(r->technology) || (r->fallback && !len)) return -1;
    return 0;
}

static inline int net_detect_request(const char *limit,const char *technology,
                                     int fallback,net_request *r)
{
    dispatch_cpu cpu=dispatch_detect_cpu();
    memset(r,0,sizeof(*r));r->machine=cpu.machine;
    r->system=UEEC_SYSTEM_LINUX;r->endian=UEEC_HOST_ENDIAN;r->fallback=fallback;
    if (limited_mask(cpu.machine,cpu.usable,limit,&r->usable)!=0) return -1;
    if (technology && strcmp(technology,"auto")) {
        if (strlen(technology)>=UEEC_TECH_SIZE) return -1;
        memcpy(r->technology,technology,strlen(technology)+1U);
    }
    uint8_t encoded[UEEC_REQUEST_SIZE];
    return net_request_encode(r,encoded);
}

static inline int net_node_matches(const file_view *file,const ueec_view *view,
                                   size_t node,const net_request *r)
{
    uint32_t w=node_word(file,view,node);
    return ((w>>7)&7U)==UEEC_MACHINE_TYPE_CPU && ((w>>10)&255U)==r->machine &&
        ((w>>18)&3U)==UEEC_SYSTEM_TYPE_GENERAL_OS && ((w>>20)&127U)==r->system &&
        (w&1U)==r->endian && (w&UINT32_C(0x1e))==UINT32_C(0x14) &&
        node_name_equals(file,view,node,"sum");
}

static inline enum net_status net_choose(const file_view *file,const ueec_view *view,
                                         const isa_candidate *candidates,
                                         const net_request *r,size_t *selected)
{
    size_t best=SIZE_MAX,requested=SIZE_MAX;
    for (size_t i=0;i<view->nodes;++i) {
        if (!net_node_matches(file,view,i,r) || (candidates[i].required & ~r->usable)) continue;
        if (best==SIZE_MAX || candidates[i].priority>candidates[best].priority) best=i;
        if (r->technology[0] && technology_matches(r->machine,r->technology,candidates[i].required) &&
            (requested==SIZE_MAX || candidates[i].priority>candidates[requested].priority)) requested=i;
    }
    if (r->technology[0] && requested==SIZE_MAX && !r->fallback) return NET_TECH_UNAVAILABLE;
    *selected=requested!=SIZE_MAX ? requested : best;
    return *selected==SIZE_MAX ? NET_NO_MATCH : NET_OK;
}

/* Canonical output: LE SH16, one node at index 0, one CODE, one shared MD.
   Source MD may be LE/BE, 16/32/64-bit, shared or legacy. Never copy the full
   shared source MD: rebuild only the selected node's validated records. */
static inline int net_export(const file_view *file,const ueec_view *view,
                             const isa_candidate *candidate,size_t node,
                             unsigned granule,uint8_t **out,size_t *out_size)
{
    section_view code;uint64_t offset;
    if (node>=view->nodes || get_section(file,view,candidate->code_section,&code)!=0 ||
        section_file_offset(file,view,candidate->code_section,&offset)!=0 ||
        offset>(uint64_t)file->size || code.data_size>(uint64_t)file->size-offset) return -1;
    uint8_t md[34]={0};
    write_u16(md+2,UEEC_MD_DESIGN_NODE_INDEXED,UEEC_ENDIAN_LITTLE);
    write_u16(md+4,2,UEEC_ENDIAN_LITTLE);
    write_u16(md+8,family_for(candidate->machine).metadata_type,UEEC_ENDIAN_LITTLE);
    write_u16(md+14,UEEC_MD_TYPE_PRIORITY,UEEC_ENDIAN_LITTLE);
    write_u16(md+16,8,UEEC_ENDIAN_LITTLE);
    write_u16(md+22,10,UEEC_ENDIAN_LITTLE);
    write_u64(md+24,candidate->required,UEEC_ENDIAN_LITTLE);
    write_u16(md+32,candidate->priority,UEEC_ENDIAN_LITTLE);
    const uint8_t *name;size_t name_size;
    node_name(file,view,node,&name,&name_size);
    if (name_size>255U) return -1;
    UEECNode n={0};n.raw=node_word(file,view,node);n.format=0;
    n.name=name;n.name_len=(uint16_t)name_size;n.ref_count=2;n.refs[0]=1;n.refs[1]=2;
    UEECSection sections[3]={{0,NULL,0},{UEEC_SECTION_CODE,file->data+(size_t)offset,(size_t)code.data_size},
                            {UEEC_SECTION_METADATA,md,sizeof(md)}};
    UEECDoc doc={1,3,&n,sections};char err[160];
    return ueec_pack(&doc,granule,out,out_size,err,sizeof(err));
}

static inline int net_validate_received(const uint8_t *data,size_t size,
                                        const net_request *request,isa_candidate *candidate)
{
    file_view file={data,size};ueec_view view;
    if (parse_ueec(&file,&view)!=0 || view.nodes!=1U || view.section_count!=3U ||
        collect_candidates(&file,&view,candidate)!=0 || !net_node_matches(&file,&view,0,request) ||
        (candidate->required & ~request->usable)) return -1;
    if (request->technology[0] && !request->fallback &&
        !technology_matches(request->machine,request->technology,candidate->required)) return -1;
    section_view section;uint64_t offset;
    if (get_section(&file,&view,candidate->metadata_section,&section)!=0 ||
        section_file_offset(&file,&view,candidate->metadata_section,&offset)!=0 ||
        section.data_size<6U || read_u16(data+(size_t)offset+2U,view.endian)!=UEEC_MD_DESIGN_NODE_INDEXED) return -1;
    return 0;
}
#endif
