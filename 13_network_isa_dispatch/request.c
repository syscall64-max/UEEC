#include "net/selection.h"

int main(int argc,char **argv)
{
    const char *output=NULL,*limit=NULL,*technology=NULL;int fallback=0;
    for (int i=1;i<argc;++i) {
        if (!strcmp(argv[i],"--fallback")) fallback=1;
        else if (i+1<argc && !strcmp(argv[i],"--output")) output=argv[++i];
        else if (i+1<argc && !strcmp(argv[i],"--limit-isa")) limit=argv[++i];
        else if (i+1<argc && !strcmp(argv[i],"--technology")) technology=argv[++i];
        else {fprintf(stderr,"Usage: %s --output FILE [--technology NAME] [--fallback] [--limit-isa PROFILE]\n",argv[0]);return 2;}
    }
    net_request r;uint8_t encoded[UEEC_REQUEST_SIZE];
    if (!output || net_detect_request(limit,technology,fallback,&r)!=0 || net_request_encode(&r,encoded)!=0) {
        fprintf(stderr,"error: invalid request options\n");return 2;
    }
    if (ueec_write_exclusive(output,encoded,sizeof(encoded))!=0) {perror("request output");return 1;}
    printf("REQUEST machine=%u architecture=%s system=%u endian=%u usable=0x%016" PRIx64 " technology=%s fallback=%d file=%s\n",
           r.machine,family_for(r.machine).architecture,r.system,r.endian,r.usable,
           r.technology[0] ? r.technology : "auto",r.fallback,output);
    return 0;
}
