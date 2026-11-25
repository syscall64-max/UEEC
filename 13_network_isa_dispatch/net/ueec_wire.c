#define _POSIX_C_SOURCE 200809L
#include "ueec_wire.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fail(char *err,size_t n,const char *fmt, ...) {
    if(err && n) { va_list ap;va_start(ap,fmt);vsnprintf(err,n,fmt,ap);va_end(ap); }
    return -1;
}
static unsigned r16(const uint8_t *p){ return (unsigned)p[0]|(unsigned)p[1]<<8; }
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void w16(uint8_t *p,unsigned v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static int within(size_t total,size_t at,size_t n){return at<=total && n<=total-at;}
static size_t aligned(size_t n, unsigned granule){return (n+(size_t)granule-1)/granule*granule;}
void ueec_free(UEECDoc *doc){if(!doc)return;free(doc->nodes);free(doc->sections);memset(doc,0,sizeof(*doc));}
int ueec_parse(const uint8_t *data,size_t size,UEECDoc *out,char *err,size_t errlen){
    if(!out)return fail(err,errlen,"null document");
    memset(out,0,sizeof(*out));
    if(!data || size<8 || size>UEEC_MAX_FILE || memcmp(data,"_GM",3)!=0 || data[3]!=1)
        return fail(err,errlen,"requires UEEC v1 little-endian (max 64 MiB)");
    unsigned n=r16(data+4),nn=r16(data+6);
    if(!n || n>UEEC_MAX_NODES || !nn || nn>UEEC_MAX_NODES)return fail(err,errlen,"unsupported node/name counts");
    UEECNode *nodes=calloc(n,sizeof(*nodes));
    if(!nodes)return fail(err,errlen,"memory allocation failed");
    size_t cur=8,total_refs=0,sections=1;
    int rc=-1;
    uint8_t *name_lengths=NULL;
    const uint8_t **name_ptrs=NULL;
    uint16_t *node_names=NULL;
    UEECSection *ss=NULL;
    size_t *section_spans=NULL;
    if(!within(size,cur,(size_t)n*6))goto bad;
    for(unsigned i=0;i<n;i++){
        uint32_t raw=r32(data+cur);uint16_t fmt=(uint16_t)r16(data+cur+4);cur+=6;
        if(fmt!=0){fail(err,errlen,"only SH16 is implemented");goto cleanup;}
        nodes[i].raw=raw;nodes[i].format=fmt;nodes[i].ref_count=(uint8_t)((raw>>27)&31);
        total_refs+=nodes[i].ref_count;
    }
    if(!within(size,cur,total_refs*6))goto bad;
    for(size_t i=0;i<total_refs;i++){
        unsigned ni=r16(data+cur);uint32_t si=r32(data+cur+2);cur+=6;
        if(ni>=n || !si || si>=UEEC_MAX_SECTIONS){fail(err,errlen,"invalid node link");goto cleanup;}
        UEECNode *nd=&nodes[ni];
        // Temporarily record links in the next available ref slot using a separate counter in nrefs.
        unsigned used=0;while(used<nd->ref_count && nd->refs[used])used++;
        if(used>=nd->ref_count){fail(err,errlen,"too many node links");goto cleanup;}
        for(unsigned j=0;j<used;j++)if(nd->refs[j]==si){fail(err,errlen,"duplicate section link");goto cleanup;}
        nd->refs[used]=si;
        if((size_t)si>=sections)sections=(size_t)si+1;
    }
    for(unsigned i=0;i<n;i++)for(unsigned j=0;j<nodes[i].ref_count;j++)
        if(!nodes[i].refs[j]){fail(err,errlen,"missing node link");goto cleanup;}
    if(sections<2 || !within(size,cur,sections*6))goto bad;
    ss=calloc(sections,sizeof(*ss));section_spans=calloc(sections,sizeof(*section_spans));
    if(!ss||!section_spans){fail(err,errlen,"memory allocation failed");goto cleanup;}
    for(size_t s=0;s<sections;s++){
        size_t span=r16(data+cur+2),len=r16(data+cur+4);
        if(len>span){fail(err,errlen,"data_size exceeds section_size");goto cleanup;}
        ss[s].kind=(uint16_t)r16(data+cur);ss[s].size=len;section_spans[s]=span;cur+=6;
    }
    if(ss[0].kind!=0){fail(err,errlen,"no HEADER section");goto cleanup;}
    if(!within(size,cur,(size_t)nn+2u*n))goto bad;
    name_lengths=malloc(nn);node_names=malloc((size_t)n*sizeof(uint16_t));name_ptrs=calloc(nn,sizeof(*name_ptrs));
    if(!name_lengths||!node_names||!name_ptrs){fail(err,errlen,"memory allocation failed");goto cleanup;}
    memcpy(name_lengths,data+cur,nn);cur+=nn;
    for(unsigned i=0;i<n;i++){node_names[i]=(uint16_t)r16(data+cur);cur+=2;if(node_names[i]>=nn){fail(err,errlen,"bad name index");goto cleanup;}}
    for(unsigned i=0;i<nn;i++){
        if(!within(size,cur,name_lengths[i]))goto bad;
        name_ptrs[i]=data+cur;cur+=name_lengths[i];
    }
    if(cur!=ss[0].size || cur>section_spans[0]){
        fail(err,errlen,"HEADER metadata mismatch");goto cleanup;
    }
    size_t off=0;
    for(size_t s=0;s<sections;s++){
        if(!within(size,off,section_spans[s])){fail(err,errlen,"truncated payload");goto cleanup;}
        ss[s].data=data+off;off+=section_spans[s];
    }
    if(off!=size){fail(err,errlen,"extra bytes after section table");goto cleanup;}
    for(unsigned i=0;i<n;i++){
        nodes[i].name=name_ptrs[node_names[i]];
        nodes[i].name_len=name_lengths[node_names[i]];
    }
    out->nodes=nodes;out->node_count=n;out->sections=ss;out->section_count=sections;
    nodes=NULL;ss=NULL;rc=0;
    goto cleanup;
bad:fail(err,errlen,"truncated or invalid UEEC metadata");
cleanup:
    free(nodes);free(ss);free(name_lengths);free(node_names);free(name_ptrs);free(section_spans);
    return rc;
}
int ueec_select(const UEECDoc *in,unsigned machine,unsigned system,UEECDoc *out,char *err,size_t elen){
    if(!in||!out)return fail(err,elen,"null document");
    memset(out,0,sizeof(*out));
    if(machine>255||system>127)return fail(err,elen,"invalid selector");
    UEECNode *nodes=calloc(in->node_count,sizeof(*nodes));
    UEECSection *secs=calloc(in->section_count,sizeof(*secs));
    uint32_t *map=calloc(in->section_count,sizeof(*map));
    if(!nodes||!secs||!map){free(nodes);free(secs);free(map);return fail(err,elen,"out of memory");}
    size_t matched=0,used=1;
    for(size_t i=0;i<in->node_count;i++){
        const UEECNode *n=&in->nodes[i];uint32_t word=n->raw;
        if(((word>>7)&7)!=0 || ((word>>10)&255)!=machine || ((word>>18)&3)!=2 || ((word>>20)&127)!=system)continue;
        nodes[matched++]=*n;
        for(unsigned j=0;j<n->ref_count;j++)map[n->refs[j]]=UINT32_MAX;
    }
    if(!matched){free(nodes);free(secs);free(map);return fail(err,elen,"no node matches requested CPU/Linux");}
    // Copy selected sections in original order. Keep node-local link order unchanged.
    secs[0]=in->sections[0];
    for(size_t i=1;i<in->section_count;i++)if(map[i]==UINT32_MAX){
        unsigned kind=in->sections[i].kind;
        // Payloads in reloc/import/export/unknown sections may contain global indexes that need rewriting.
        if(kind!=1 && kind!=2 && kind!=3 && kind!=4){
            free(nodes);free(secs);free(map);
            return fail(err,elen,"unsafe referenced section type %u: rewriter required",kind);
        }
        map[i]=(uint32_t)used;secs[used++]=in->sections[i];
    }
    for(size_t i=0;i<matched;i++)for(unsigned j=0;j<nodes[i].ref_count;j++)
        nodes[i].refs[j]=map[nodes[i].refs[j]];
    out->nodes=nodes;out->node_count=matched;out->sections=secs;out->section_count=used;
    free(map);return 0;
}
int ueec_pack(const UEECDoc *doc,unsigned g,uint8_t **result,size_t *result_size,char *err,size_t elen){
    if(!doc || !doc->nodes || !doc->sections || !result || !result_size)return fail(err,elen,"invalid arguments");
    *result=NULL;*result_size=0;
    if(!g || (g&(g-1)) || g>32768)return fail(err,elen,"granule must be power of two <=32768");
    if(!doc->node_count || doc->node_count>UEEC_MAX_NODES || doc->section_count<2 || doc->section_count>UEEC_MAX_SECTIONS)
        return fail(err,elen,"invalid node/section counts");
    // Every node gets exactly one name slot (safe even for repeated names).
    const size_t n=doc->node_count,s=doc->section_count;
    size_t links=0,nbytes=0;
    for(size_t i=0;i<n;i++){
        if(doc->nodes[i].ref_count>UEEC_MAX_REFS || doc->nodes[i].format!=0)return fail(err,elen,"invalid node format");
        links+=doc->nodes[i].ref_count;
        nbytes+=doc->nodes[i].name_len;
        if(doc->nodes[i].name_len>255)return fail(err,elen,"name too long");
        for(unsigned j=0;j<doc->nodes[i].ref_count;j++){
            if(!doc->nodes[i].refs[j] || doc->nodes[i].refs[j]>=s)return fail(err,elen,"bad section reference");
        }
    }
    size_t header=8+6*n+6*links+6*s+n+2*n+nbytes;
    if(header>65535)return fail(err,elen,"SH16 header overflow");
    size_t hspan=aligned(header,g);
    if(hspan>65535)return fail(err,elen,"SH16 header padding overflow");
    size_t total=hspan;
    for(size_t i=1;i<s;i++){
        if(doc->sections[i].size>65535)return fail(err,elen,"SH16 data section overflow");
        size_t span=aligned(doc->sections[i].size,g);
        if(span>65535 || total>UEEC_MAX_FILE-span)return fail(err,elen,"SH16 granulation/file size overflow");
        total+=span;
    }
    if(total>UEEC_MAX_FILE)return fail(err,elen,"UEEC output >64 MiB");
    uint8_t *b=calloc(total?total:1,1);if(!b)return fail(err,elen,"out of memory");
    memcpy(b,"_GM\x01",4);w16(b+4,(unsigned)n);w16(b+6,(unsigned)n);
    size_t pos=8;
    for(size_t i=0;i<n;i++){
        uint32_t word=(doc->nodes[i].raw&~(31u<<27))|((uint32_t)doc->nodes[i].ref_count<<27);
        w32(b+pos,word);w16(b+pos+4,0);pos+=6;
    }
    for(size_t i=0;i<n;i++)for(unsigned j=0;j<doc->nodes[i].ref_count;j++){
        w16(b+pos,(unsigned)i);w32(b+pos+2,doc->nodes[i].refs[j]);pos+=6;
    }
    for(size_t i=0;i<s;i++){
        size_t span=i?aligned(doc->sections[i].size,g):hspan;
        w16(b+pos,doc->sections[i].kind);w16(b+pos+2,(unsigned)span);
        w16(b+pos+4,(unsigned)(i?doc->sections[i].size:header));pos+=6;
    }
    for(size_t i=0;i<n;i++)b[pos++]=(uint8_t)doc->nodes[i].name_len;
    for(size_t i=0;i<n;i++){w16(b+pos,(unsigned)i);pos+=2;}
    for(size_t i=0;i<n;i++){
        if(doc->nodes[i].name_len) memcpy(b+pos,doc->nodes[i].name,doc->nodes[i].name_len);
        pos+=doc->nodes[i].name_len;
    }
    if(pos!=header){free(b);return fail(err,elen,"pack metadata logic error");}
    pos=hspan;
    for(size_t i=1;i<s;i++){
        if(doc->sections[i].size) memcpy(b+pos,doc->sections[i].data,doc->sections[i].size);
        pos+=aligned(doc->sections[i].size,g);
    }
    *result=b;*result_size=total;return 0;
}
int ueec_read_file(const char *path,uint8_t **buffer,size_t *n){
    *buffer=NULL;*n=0;int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;
    struct stat st;if(fstat(fd,&st)||st.st_size<8||(uint64_t)st.st_size>UEEC_MAX_FILE){close(fd);errno=EINVAL;return -1;}
    size_t sz=(size_t)st.st_size;uint8_t *p=malloc(sz);if(!p){close(fd);return -1;}
    size_t off=0;while(off<sz){ssize_t got=read(fd,p+off,sz-off);if(got<0&&errno==EINTR)continue;if(got<=0){free(p);close(fd);return -1;}off+=(size_t)got;}
    close(fd);*buffer=p;*n=sz;return 0;
}
int ueec_write_exclusive(const char *path,const uint8_t *p,size_t n){
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0644);if(fd<0)return -1;
    size_t off=0;while(off<n){ssize_t got=write(fd,p+off,n-off);if(got<0&&errno==EINTR)continue;if(got<=0){close(fd);unlink(path);return -1;}off+=(size_t)got;}
    if(close(fd)!=0){unlink(path);return -1;}return 0;
}
