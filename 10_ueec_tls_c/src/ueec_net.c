#define _POSIX_C_SOURCE 200809L
#include "ueec_wire.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define PROTO "UEC1"
#define LOCAL_ADDR "127.0.0.1"
#define MAX_PORT 65535

typedef struct {
    const char *source,*cert,*key,*ca,*output,*mode;
    unsigned machine,system,granule,port;
    int machine_set,client;
} Config;

static void syntax(const char *app){
    fprintf(stderr,"Usage:\n"
      "  %s server --source multi.ueec --cert cert.pem --key key.pem [--port 9443]\n"
      "  %s client --machine N --ca cert.pem --output result.ueec --mode compact|granulated [--granule 4096] [--system 2] [--port 9443]\n"
      "Localhost-only TLS demo: UEEC v1 little-endian SH16, CPU/Linux nodes.\n",app,app);
}
static int number(const char *value,unsigned max,unsigned *out){
    if(!value || !*value || *value=='-' || *value=='+')return -1;
    char *end=NULL;errno=0;unsigned long n=strtoul(value,&end,10);
    if(errno || !end || *end || n>max)return -1;
    *out=(unsigned)n;return 0;
}
static int parse_args(int argc,char **argv,Config *a){
    memset(a,0,sizeof(*a));a->port=9443;a->system=2;a->mode="compact";
    long pagesize=sysconf(_SC_PAGESIZE);a->granule=pagesize>0 && pagesize<=32768?(unsigned)pagesize:4096;
    if(argc<2)return -1;
    if(!strcmp(argv[1],"server"))a->client=0;
    else if(!strcmp(argv[1],"client"))a->client=1;
    else return -1;
    for(int i=2;i<argc;i++){
        if(i+1>=argc)return -1;
        const char *k=argv[i],*v=argv[++i];
        if(!strcmp(k,"--source"))a->source=v;
        else if(!strcmp(k,"--cert"))a->cert=v;
        else if(!strcmp(k,"--key"))a->key=v;
        else if(!strcmp(k,"--ca"))a->ca=v;
        else if(!strcmp(k,"--output"))a->output=v;
        else if(!strcmp(k,"--mode"))a->mode=v;
        else if(!strcmp(k,"--machine")){if(number(v,255,&a->machine))return -1;a->machine_set=1;}
        else if(!strcmp(k,"--system")){if(number(v,127,&a->system))return -1;}
        else if(!strcmp(k,"--port")){if(number(v,MAX_PORT,&a->port)||a->port==0)return -1;}
        else if(!strcmp(k,"--granule")){if(number(v,32768,&a->granule))return -1;}
        else return -1;
    }
    if(a->client){
        if(!a->ca||!a->output||!a->machine_set)return -1;
        if(strcmp(a->mode,"compact") && strcmp(a->mode,"granulated"))return -1;
        if(!a->granule || (a->granule&(a->granule-1)))return -1;
    }else if(!a->source||!a->cert||!a->key)return -1;
    return 0;
}
static int ssl_write_all(SSL *s,const uint8_t *p,size_t n){
    for(size_t at=0;at<n;){
        int len=n-at>16384?16384:(int)(n-at);int w=SSL_write(s,p+at,len);
        if(w<=0)return -1;
        at+=(size_t)w;
    }return 0;
}
static int ssl_read_all(SSL *s,uint8_t *p,size_t n){
    for(size_t at=0;at<n;){
        int len=n-at>16384?16384:(int)(n-at);int r=SSL_read(s,p+at,len);
        if(r<=0)return -1;
        at+=(size_t)r;
    }return 0;
}
static void digest(const uint8_t *p,size_t n,uint8_t hash[32]){
    unsigned hlen=0;
    if(EVP_Digest(p,n,hash,&hlen,EVP_sha256(),NULL)!=1 || hlen!=32){
        fprintf(stderr,"SHA-256 failure\n");exit(2);
    }
}
static int open_loopback(unsigned port,int server){
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0)return -1;
    struct timeval to={.tv_sec=30,.tv_usec=0};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&to,sizeof(to));
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;
    addr.sin_port=htons((uint16_t)port);inet_pton(AF_INET,LOCAL_ADDR,&addr.sin_addr);
    if(server){int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        if(bind(fd,(void *)&addr,sizeof(addr))<0 || listen(fd,16)<0){close(fd);return -1;}
    }else if(connect(fd,(void *)&addr,sizeof(addr))<0){close(fd);return -1;}
    return fd;
}
static void send_failure(SSL *s){uint8_t r[40]={0};memcpy(r,PROTO,4);(void)ssl_write_all(s,r,sizeof(r));}
static int serve_request(SSL *s,const UEECDoc *doc,size_t orig_bytes){
    uint8_t req[8];char err[240]={0};
    if(ssl_read_all(s,req,sizeof(req))!=0 || memcmp(req,PROTO,4) || req[6] || req[7]){
        fprintf(stderr,"Invalid protocol request\n");send_failure(s);return -1;
    }
    unsigned machine=req[4],system=req[5];UEECDoc selected={0};
    uint8_t *payload=NULL;size_t length=0;
    if(ueec_select(doc,machine,system,&selected,err,sizeof(err)) ||
       ueec_pack(&selected,1,&payload,&length,err,sizeof(err))){
        fprintf(stderr,"Selector %u/%u rejected: %s\n",machine,system,err);
        send_failure(s);ueec_free(&selected);return -1;
    }
    if(length>UINT32_MAX){send_failure(s);free(payload);ueec_free(&selected);return -1;}
    uint8_t head[40]={0};memcpy(head,PROTO,4);
    uint32_t b=htonl((uint32_t)length);memcpy(head+4,&b,4);
    digest(payload,length,head+8);
    int rc=ssl_write_all(s,head,sizeof(head)) || ssl_write_all(s,payload,length);
    fprintf(stdout,"SERVE machine=%u system=%u nodes=%zu source=%zuB payload=%zuB status=%s\n",
        machine,system,selected.node_count,orig_bytes,length,rc?"FAIL":"PASS");fflush(stdout);
    free(payload);ueec_free(&selected);return rc?-1:0;
}
static int server(const Config *a){
    int rc=1;uint8_t *blob=NULL;size_t length=0;UEECDoc doc={0};char err[256]={0};
    SSL_CTX *ctx=NULL;int fd=-1;
    if(ueec_read_file(a->source,&blob,&length)){
        perror("source");goto done;
    }
    if(ueec_parse(blob,length,&doc,err,sizeof(err))){fprintf(stderr,"Invalid source: %s\n",err);goto done;}
    ctx=SSL_CTX_new(TLS_server_method());
    if(!ctx || SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)!=1 ||
       SSL_CTX_use_certificate_file(ctx,a->cert,SSL_FILETYPE_PEM)!=1 ||
       SSL_CTX_use_PrivateKey_file(ctx,a->key,SSL_FILETYPE_PEM)!=1 ||
       SSL_CTX_check_private_key(ctx)!=1){fprintf(stderr,"TLS server certificate/key configuration failed\n");ERR_print_errors_fp(stderr);goto done;}
    fd=open_loopback(a->port,1);if(fd<0){perror("bind/listen");goto done;}
    printf("TLS UEEC server listening on %s:%u, source=%s, nodes=%zu\n",LOCAL_ADDR,a->port,a->source,doc.node_count);
    fflush(stdout);rc=0;
    for(;;){
        int client=accept(fd,NULL,NULL);
        if(client<0){if(errno==EINTR)continue;perror("accept");rc=1;break;}
        struct timeval to={.tv_sec=30,.tv_usec=0};
        setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to));
        setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&to,sizeof(to));
        SSL *ssl=SSL_new(ctx);
        if(ssl){SSL_set_fd(ssl,client);
            if(SSL_accept(ssl)==1) (void)serve_request(ssl,&doc,length);
            else fprintf(stderr,"TLS handshake failed\n");
            SSL_shutdown(ssl);SSL_free(ssl);
        }close(client);
    }
done:
    if(fd>=0)close(fd);
    SSL_CTX_free(ctx);ueec_free(&doc);free(blob);return rc;
}
static int client(const Config *a){
    int rc=1,fd=-1;SSL_CTX *ctx=NULL;SSL *ssl=NULL;uint8_t *payload=NULL,*output=NULL;
    size_t outlen=0;UEECDoc parsed={0};char err[256]={0};
    ctx=SSL_CTX_new(TLS_client_method());
    if(!ctx || SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)!=1 ||
       SSL_CTX_load_verify_locations(ctx,a->ca,NULL)!=1){
        fprintf(stderr,"TLS trust certificate failure\n");goto done;
    }
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);
    fd=open_loopback(a->port,0);if(fd<0){perror("connect");goto done;}
    ssl=SSL_new(ctx);
    if(!ssl || SSL_set_fd(ssl,fd)!=1 || SSL_set_tlsext_host_name(ssl,"localhost")!=1 ||
       SSL_set1_host(ssl,"localhost")!=1 || SSL_connect(ssl)!=1 || SSL_get_verify_result(ssl)!=X509_V_OK){
        fprintf(stderr,"TLS connect or certificate hostname validation failed\n");ERR_print_errors_fp(stderr);goto done;
    }
    uint8_t req[8]={0};memcpy(req,PROTO,4);req[4]=(uint8_t)a->machine;req[5]=(uint8_t)a->system;
    uint8_t head[40]={0};
    if(ssl_write_all(ssl,req,sizeof(req)) || ssl_read_all(ssl,head,sizeof(head)) || memcmp(head,PROTO,4)){
        fprintf(stderr,"Protocol header failed\n");goto done;
    }
    uint32_t netlen;memcpy(&netlen,head+4,4);size_t length=ntohl(netlen);
    if(!length || length>UEEC_MAX_FILE){fprintf(stderr,"Server rejected request or returned oversized payload\n");goto done;}
    payload=malloc(length);if(!payload){fprintf(stderr,"OOM\n");goto done;}
    if(ssl_read_all(ssl,payload,length)){fprintf(stderr,"Incomplete TLS payload\n");goto done;}
    uint8_t sha[32];digest(payload,length,sha);
    if(CRYPTO_memcmp(sha,head+8,32)){fprintf(stderr,"SHA-256 mismatch\n");goto done;}
    if(ueec_parse(payload,length,&parsed,err,sizeof(err))){fprintf(stderr,"Received invalid UEEC: %s\n",err);goto done;}
    for(size_t i=0;i<parsed.node_count;i++){
        uint32_t raw=parsed.nodes[i].raw;
        if(((raw>>7)&7)!=0 || ((raw>>10)&255)!=a->machine || ((raw>>18)&3)!=2 || ((raw>>20)&127)!=a->system){
            fprintf(stderr,"Received UEEC contains unexpected node\n");goto done;
        }
    }
    unsigned granule=!strcmp(a->mode,"compact")?1:a->granule;
    if(ueec_pack(&parsed,granule,&output,&outlen,err,sizeof(err))){fprintf(stderr,"Repack failed: %s\n",err);goto done;}
    if(ueec_write_exclusive(a->output,output,outlen)){perror("write output (will not overwrite)");goto done;}
    printf("CLIENT machine=%u system=%u TLS payload=%zuB output=%zuB nodes=%zu mode=%s granule=%u file=%s\n",
        a->machine,a->system,length,outlen,parsed.node_count,a->mode,granule,a->output);
    rc=0;
done:
    ueec_free(&parsed);free(payload);free(output);
    if(ssl){SSL_shutdown(ssl);SSL_free(ssl);}
    if(fd>=0)close(fd);
    SSL_CTX_free(ctx);return rc;
}
int main(int argc,char **argv){
    signal(SIGPIPE,SIG_IGN);
    Config conf; if(parse_args(argc,argv,&conf)){syntax(argv[0]);return 2;}
    return conf.client?client(&conf):server(&conf);
}
