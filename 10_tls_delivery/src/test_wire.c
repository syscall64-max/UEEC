/* Unit tests: original 11-node assembler fixture, 11 selections,
 * shared physical DATA, roundtrip, malformed/truncated input rejection. */
#include "ueec_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if(!(expr)) {fprintf(stderr,"FAIL at line %d: %s\n",__LINE__,#expr);return 1;} } while(0)

int main(int argc,char **argv) {
    if(argc!=2){fprintf(stderr,"Usage: %s fixture.ueec\n",argv[0]);return 2;}
    uint8_t *src=NULL;size_t sz=0;char err[256]={0};UEECDoc doc={0};
    CHECK(ueec_read_file(argv[1],&src,&sz)==0);
    CHECK(ueec_parse(src,sz,&doc,err,sizeof(err))==0);
    CHECK(doc.node_count==11);
    for(unsigned arch=0;arch<11;arch++) {
        UEECDoc selected={0},reparsed={0};uint8_t *compact=NULL,*granulated=NULL;
        size_t cbytes=0,gbytes=0;
        CHECK(ueec_select(&doc,arch,2,&selected,err,sizeof(err))==0);
        CHECK(selected.node_count==1);
        CHECK(selected.section_count==2);
        CHECK(ueec_pack(&selected,1,&compact,&cbytes,err,sizeof(err))==0);
        CHECK(ueec_pack(&selected,4096,&granulated,&gbytes,err,sizeof(err))==0);
        CHECK(cbytes<sz && gbytes>=cbytes);
        CHECK(ueec_parse(compact,cbytes,&reparsed,err,sizeof(err))==0);
        CHECK(reparsed.node_count==1 && reparsed.section_count==2);
        CHECK(reparsed.nodes[0].refs[0]==1);
        CHECK(reparsed.sections[1].size==selected.sections[1].size);
        CHECK(memcmp(reparsed.sections[1].data,selected.sections[1].data,selected.sections[1].size)==0);
        ueec_free(&reparsed);
        CHECK(ueec_parse(granulated,gbytes,&reparsed,err,sizeof(err))==0);
        CHECK(reparsed.nodes[0].refs[0]==1);
        CHECK(reparsed.sections[1].size==selected.sections[1].size);
        CHECK(memcmp(reparsed.sections[1].data,selected.sections[1].data,selected.sections[1].size)==0);
        ueec_free(&reparsed);ueec_free(&selected);free(compact);free(granulated);
        printf("PASS wire arch=%u compact/granulated\n",arch);
    }
    // Synthetic shared physical section: both chosen nodes refer to one global DATA section.
    const uint8_t name[]="demo",a[]={0x90},b[]={0x90,0x90},shared[]={1,2,3,4,5};
    UEECNode nd[2]={0};UEECSection secs[4]={0};
    for(unsigned i=0;i<2;i++){
        nd[i].raw=(uint32_t)((2u<<20)|(2u<<18)|(10u<<10)|(1u<<3)|(1u<<2)|(2u<<27));
        nd[i].name=name;nd[i].name_len=4;nd[i].ref_count=2;
        nd[i].refs[0]=i+1;nd[i].refs[1]=3;
    }
    secs[1]=(UEECSection){.kind=1,.data=a,.size=sizeof(a)};
    secs[2]=(UEECSection){.kind=1,.data=b,.size=sizeof(b)};
    secs[3]=(UEECSection){.kind=4,.data=shared,.size=sizeof(shared)};
    UEECDoc shared_doc={.node_count=2,.section_count=4,.nodes=nd,.sections=secs};
    UEECDoc selected={0},reparsed={0};uint8_t *packed=NULL;size_t plen=0;
    CHECK(ueec_select(&shared_doc,10,2,&selected,err,sizeof(err))==0);
    CHECK(selected.node_count==2 && selected.section_count==4);
    CHECK(selected.nodes[0].refs[1]==3 && selected.nodes[1].refs[1]==3);
    CHECK(ueec_pack(&selected,1,&packed,&plen,err,sizeof(err))==0);
    CHECK(ueec_parse(packed,plen,&reparsed,err,sizeof(err))==0);
    CHECK(reparsed.nodes[0].refs[1]==3 && reparsed.nodes[1].refs[1]==3);
    CHECK(!memcmp(reparsed.sections[3].data,shared,sizeof(shared)));
    ueec_free(&selected);ueec_free(&reparsed);free(packed);
    // Malformed and truncated input must be rejected.
    CHECK(ueec_parse(src,sz-1,&reparsed,err,sizeof(err))!=0);
    CHECK(ueec_parse(src,5,&reparsed,err,sizeof(err))!=0);
    uint8_t *bad=malloc(sz);CHECK(bad!=NULL);memcpy(bad,src,sz);bad[0]=0;
    CHECK(ueec_parse(bad,sz,&reparsed,err,sizeof(err))!=0);
    free(bad);
    ueec_free(&doc);free(src);
    puts("PASS: 11 selectors, shared sections, malformed inputs");return 0;
}
