#include "selection.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>

typedef struct {
    const char *source,*cert,*key,*ca,*output,*mode,*host,*bind_address;
    const char *request_file,*technology,*limit;
    unsigned port,granule,max_requests;
    int client,fallback;
} net_config;

static void net_usage(const char *app)
{
    fprintf(stderr,"Usage:\n"
        "  %s server --source FILE --cert CERT --key KEY [--bind ADDRESS] [--port 9443]\n"
        "  %s client --ca CERT --output FILE [--host localhost] [--port 9443]\n"
        "      [--technology NAME] [--fallback] [--mode compact|granulated] [--granule 4096]\n"
        "      [--limit-isa PROFILE] [--request-file FILE]\n"
        "CPU architecture, Linux OS and OS-usable ISA are detected automatically.\n"
        "--request-file transports a target-generated profile; it does not execute foreign CODE.\n",app,app);
}

static int number(const char *s,unsigned max,unsigned *out)
{
    if (!s || !*s || *s=='-' || *s=='+') return -1;
    char *end;errno=0;unsigned long n=strtoul(s,&end,10);
    if (errno || *end || n>max) return -1;
    *out=(unsigned)n;return 0;
}

static int parse_args(int argc,char **argv,net_config *c)
{
    memset(c,0,sizeof(*c));c->host="localhost";c->bind_address="127.0.0.1";
    c->port=9443;c->granule=4096;c->mode="compact";
    if (argc<2) return -1;
    if (!strcmp(argv[1],"client")) c->client=1;
    else if (strcmp(argv[1],"server")) return -1;
    for (int i=2;i<argc;++i) {
        const char *k=argv[i];
        if (!strcmp(k,"--fallback")) {c->fallback=1;continue;}
        if (i+1>=argc) return -1;
        const char *v=argv[++i];
        if (!strcmp(k,"--source")) c->source=v;
        else if (!strcmp(k,"--cert")) c->cert=v;
        else if (!strcmp(k,"--key")) c->key=v;
        else if (!strcmp(k,"--ca")) c->ca=v;
        else if (!strcmp(k,"--output")) c->output=v;
        else if (!strcmp(k,"--host")) c->host=v;
        else if (!strcmp(k,"--bind")) c->bind_address=v;
        else if (!strcmp(k,"--request-file")) c->request_file=v;
        else if (!strcmp(k,"--technology")) c->technology=v;
        else if (!strcmp(k,"--limit-isa")) c->limit=v;
        else if (!strcmp(k,"--mode")) c->mode=v;
        else if (!strcmp(k,"--port")) {if (number(v,65535,&c->port)) return -1;}
        else if (!strcmp(k,"--granule")) {if (number(v,32768,&c->granule)) return -1;}
        else if (!strcmp(k,"--max-requests")) {if (number(v,1000000,&c->max_requests)) return -1;}
        else return -1;
    }
    if (c->client) {
        if (!c->ca || !c->output || !c->port || !c->granule || (c->granule&(c->granule-1U)) ||
            (strcmp(c->mode,"compact") && strcmp(c->mode,"granulated"))) return -1;
        if (c->request_file && (c->technology || c->limit || c->fallback)) return -1;
    } else if (!c->source || !c->cert || !c->key) return -1;
    return 0;
}

static int socket_timeout(int fd)
{
    struct timeval timeout={.tv_sec=30,.tv_usec=0};
    return setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) ||
           setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
}

static int open_socket(const net_config *c,int server,unsigned *actual_port)
{
    char service[16];snprintf(service,sizeof(service),"%u",c->port);
    struct addrinfo hints={0},*addresses=NULL;hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;
    if (server) hints.ai_flags=AI_PASSIVE;
    int e=getaddrinfo(server ? c->bind_address : c->host,service,&hints,&addresses);
    if (e) {fprintf(stderr,"address: %s\n",gai_strerror(e));return -1;}
    int fd=-1;
    for (struct addrinfo *a=addresses;a;a=a->ai_next) {
        fd=socket(a->ai_family,a->ai_socktype,a->ai_protocol);
        if (fd<0) continue;
        if (server) {
            int yes=1;
            if (!setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)) &&
                !bind(fd,a->ai_addr,a->ai_addrlen) && !listen(fd,16)) break;
        } else if (!socket_timeout(fd) && !connect(fd,a->ai_addr,a->ai_addrlen)) break;
        close(fd);fd=-1;
    }
    freeaddrinfo(addresses);
    if (server && fd>=0) {
        struct sockaddr_storage address;socklen_t size=sizeof(address);
        if (getsockname(fd,(struct sockaddr *)&address,&size)) {close(fd);return -1;}
        if (address.ss_family==AF_INET) *actual_port=ntohs(((struct sockaddr_in *)&address)->sin_port);
        else *actual_port=ntohs(((struct sockaddr_in6 *)&address)->sin6_port);
    }
    /* No receive timeout on the listening socket: idle time is not an error. */
    return fd;
}

static int tls_write(SSL *ssl,const uint8_t *p,size_t n)
{
    for (size_t at=0;at<n;) {
        int length=n-at>16384U ? 16384 : (int)(n-at);
        int done=SSL_write(ssl,p+at,length);
        if (done<=0) return -1;
        at+=(size_t)done;
    }
    return 0;
}

static int tls_read(SSL *ssl,uint8_t *p,size_t n)
{
    for (size_t at=0;at<n;) {
        int length=n-at>16384U ? 16384 : (int)(n-at);
        int done=SSL_read(ssl,p+at,length);
        if (done<=0) return -1;
        at+=(size_t)done;
    }
    return 0;
}

static int sha256(const uint8_t *p,size_t n,uint8_t hash[32])
{
    unsigned size=0;
    return EVP_Digest(p,n,hash,&size,EVP_sha256(),NULL)==1 && size==32U ? 0 : -1;
}

static const char *status_name(unsigned status)
{
    switch (status) {
    case NET_OK:return "ok";
    case NET_BAD_REQUEST:return "bad-request";
    case NET_NO_MATCH:return "no-compatible-node";
    case NET_TECH_UNAVAILABLE:return "technology-unavailable";
    default:return "internal-error";
    }
}

static int send_rejection(SSL *ssl,enum net_status status)
{
    uint8_t head[UEEC_RESPONSE_SIZE]={0};memcpy(head,UEEC_NET_MAGIC,4);
    write_u32(head+4,(uint32_t)status,UEEC_ENDIAN_BIG);
    return tls_write(ssl,head,sizeof(head));
}

static int serve_one(SSL *ssl,const file_view *file,const ueec_view *view,
                      const isa_candidate *candidates)
{
    uint8_t encoded[UEEC_REQUEST_SIZE];net_request request;
    if (tls_read(ssl,encoded,sizeof(encoded)) || net_request_decode(encoded,&request)) {
        (void)send_rejection(ssl,NET_BAD_REQUEST);return -1;
    }
    size_t selected;enum net_status status=net_choose(file,view,candidates,&request,&selected);
    if (status!=NET_OK) {
        fprintf(stdout,"REJECT machine=%u system=%u status=%s\n",request.machine,request.system,status_name((unsigned)status));
        fflush(stdout);(void)send_rejection(ssl,status);return -1;
    }
    uint8_t *payload=NULL;size_t size=0;
    if (net_export(file,view,&candidates[selected],selected,1,&payload,&size) || size>UINT32_MAX) {
        free(payload);(void)send_rejection(ssl,NET_INTERNAL);return -1;
    }
    uint8_t head[UEEC_RESPONSE_SIZE]={0};memcpy(head,UEEC_NET_MAGIC,4);
    write_u32(head+8,(uint32_t)size,UEEC_ENDIAN_BIG);
    write_u16(head+12,(uint16_t)selected,UEEC_ENDIAN_BIG);
    write_u64(head+16,candidates[selected].required,UEEC_ENDIAN_BIG);
    write_u16(head+24,candidates[selected].priority,UEEC_ENDIAN_BIG);
    if (sha256(payload,size,head+32)) {free(payload);(void)send_rejection(ssl,NET_INTERNAL);return -1;}
    int rc=tls_write(ssl,head,sizeof(head)) || tls_write(ssl,payload,size);
    printf("SERVE machine=%u system=%u source_node=%zu technology=%s nodes=1 source=%zuB payload=%zuB status=%s\n",
           request.machine,request.system,selected,technology_of(request.machine,candidates[selected].required),
           file->size,size,rc ? "FAIL" : "PASS");fflush(stdout);
    free(payload);return rc ? -1 : 0;
}

static int run_server(const net_config *c)
{
    uint8_t *data=NULL;size_t size=0;ueec_view view;isa_candidate *candidates=NULL;
    SSL_CTX *ctx=NULL;int fd=-1,rc=1;unsigned port=0;
    if (ueec_read_file(c->source,&data,&size)) {perror("source");goto done;}
    file_view file={data,size};
    if (parse_ueec(&file,&view)) goto done;
    candidates=calloc(view.nodes,sizeof(*candidates));
    if (!candidates || collect_candidates(&file,&view,candidates)) {
        fprintf(stderr,"error: malformed source CODE/METADATA\n");goto done;
    }
    ctx=SSL_CTX_new(TLS_server_method());
    if (!ctx || SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)!=1 ||
        SSL_CTX_use_certificate_file(ctx,c->cert,SSL_FILETYPE_PEM)!=1 ||
        SSL_CTX_use_PrivateKey_file(ctx,c->key,SSL_FILETYPE_PEM)!=1 || SSL_CTX_check_private_key(ctx)!=1) {
        fprintf(stderr,"TLS server configuration failed\n");ERR_print_errors_fp(stderr);goto done;
    }
    fd=open_socket(c,1,&port);
    if (fd<0) {perror("bind/listen");goto done;}
    printf("TLS server host=%s port=%u source_nodes=%zu\n",c->bind_address,port,view.nodes);fflush(stdout);
    rc=0;unsigned requests=0;
    while (!c->max_requests || requests<c->max_requests) {
        int client=accept(fd,NULL,NULL);
        if (client<0) {if (errno==EINTR) continue;perror("accept");rc=1;break;}
        ++requests;
        SSL *ssl=SSL_new(ctx);
        if (ssl && !socket_timeout(client) && SSL_set_fd(ssl,client)==1) {
            if (SSL_accept(ssl)==1) (void)serve_one(ssl,&file,&view,candidates);
            else fprintf(stderr,"TLS handshake failed\n");
            (void)SSL_shutdown(ssl);
        }
        SSL_free(ssl);close(client);
    }
done:
    if (fd>=0) close(fd);
    SSL_CTX_free(ctx);free(candidates);free(data);return rc;
}

static int run_client(const net_config *c)
{
    net_request request;uint8_t encoded[UEEC_REQUEST_SIZE],head[UEEC_RESPONSE_SIZE];
    uint8_t *payload=NULL,*output=NULL,*request_data=NULL;size_t request_size=0,output_size=0;
    int fd=-1,rc=1;SSL_CTX *ctx=NULL;SSL *ssl=NULL;UEECDoc doc={0};char err[160];
    if (c->request_file) {
        if (ueec_read_file(c->request_file,&request_data,&request_size) || request_size!=sizeof(encoded) ||
            net_request_decode(request_data,&request)) {fprintf(stderr,"invalid request file\n");goto done;}
        memcpy(encoded,request_data,sizeof(encoded));
    } else if (net_detect_request(c->limit,c->technology,c->fallback,&request) ||
               net_request_encode(&request,encoded)) {fprintf(stderr,"invalid CPU/ISA options\n");goto done;}
    ctx=SSL_CTX_new(TLS_client_method());
    if (!ctx || SSL_CTX_set_min_proto_version(ctx,TLS1_2_VERSION)!=1 ||
        SSL_CTX_load_verify_locations(ctx,c->ca,NULL)!=1) {fprintf(stderr,"TLS trust configuration failed\n");goto done;}
    SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,NULL);
    fd=open_socket(c,0,NULL);
    if (fd<0) {perror("connect");goto done;}
    ssl=SSL_new(ctx);if (!ssl || SSL_set_fd(ssl,fd)!=1) goto done;
    uint8_t address[16];int is_ip=inet_pton(AF_INET,c->host,address)==1 || inet_pton(AF_INET6,c->host,address)==1;
    int identity_ok=is_ip ? X509_VERIFY_PARAM_set1_ip_asc(SSL_get0_param(ssl),c->host) :
        (SSL_set_tlsext_host_name(ssl,c->host)==1 && SSL_set1_host(ssl,c->host)==1);
    if (identity_ok!=1 || SSL_connect(ssl)!=1 || SSL_get_verify_result(ssl)!=X509_V_OK) {
        fprintf(stderr,"TLS certificate/hostname validation failed\n");ERR_print_errors_fp(stderr);goto done;
    }
    if (tls_write(ssl,encoded,sizeof(encoded)) || tls_read(ssl,head,sizeof(head)) || memcmp(head,UEEC_NET_MAGIC,4)) {
        fprintf(stderr,"protocol response failed\n");goto done;
    }
    unsigned status=read_u32(head+4,UEEC_ENDIAN_BIG);
    if (status!=NET_OK) {fprintf(stderr,"REJECT status=%s\n",status_name(status));goto done;}
    size_t size=read_u32(head+8,UEEC_ENDIAN_BIG);
    if (!size || size>UEEC_MAX_FILE || read_u16(head+14,UEEC_ENDIAN_BIG)) {
        fprintf(stderr,"invalid response framing\n");goto done;
    }
    for (size_t i=26;i<32;++i) if (head[i]) {
        fprintf(stderr,"invalid response framing\n");goto done;
    }
    payload=malloc(size);if (!payload || tls_read(ssl,payload,size)) {fprintf(stderr,"incomplete TLS payload\n");goto done;}
    uint8_t hash[32];
    if (sha256(payload,size,hash) || CRYPTO_memcmp(hash,head+32,32)) {fprintf(stderr,"SHA-256 mismatch\n");goto done;}
    isa_candidate candidate={0};
    if (net_validate_received(payload,size,&request,&candidate) ||
        candidate.required!=read_u64(head+16,UEEC_ENDIAN_BIG) || candidate.priority!=read_u16(head+24,UEEC_ENDIAN_BIG)) {
        fprintf(stderr,"received node does not match CPU/OS/ISA request\n");goto done;
    }
    if (ueec_parse(payload,size,&doc,err,sizeof(err))) {fprintf(stderr,"received file: %s\n",err);goto done;}
    unsigned granule=!strcmp(c->mode,"compact") ? 1U : c->granule;
    if (ueec_pack(&doc,granule,&output,&output_size,err,sizeof(err))) {fprintf(stderr,"reconstruction: %s\n",err);goto done;}
    isa_candidate reconstructed={0};
    if (net_validate_received(output,output_size,&request,&reconstructed)) {fprintf(stderr,"reconstructed node invalid\n");goto done;}
    if (ueec_write_exclusive(c->output,output,output_size)) {perror("output (will not overwrite)");goto done;}
    printf("CLIENT machine=%u system=%u technology=%s source_node=%u output_node=0 nodes=1 "
           "TLS_payload=%zuB output=%zuB mode=%s granule=%u file=%s\n",
           request.machine,request.system,technology_of(request.machine,candidate.required),
           (unsigned)read_u16(head+12,UEEC_ENDIAN_BIG),size,output_size,c->mode,granule,c->output);
    rc=0;
done:
    ueec_free(&doc);free(request_data);free(payload);free(output);
    if (ssl) (void)SSL_shutdown(ssl);
    SSL_free(ssl);if (fd>=0) close(fd);SSL_CTX_free(ctx);return rc;
}

int main(int argc,char **argv)
{
    signal(SIGPIPE,SIG_IGN);net_config config;
    if (parse_args(argc,argv,&config)) {net_usage(argv[0]);return 2;}
    return config.client ? run_client(&config) : run_server(&config);
}
