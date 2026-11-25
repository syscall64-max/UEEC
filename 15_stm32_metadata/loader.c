#define main ueec_reference_main
#include "reference/ueec_load.c"
#undef main
#include "ueec_stm32_metadata.h"

typedef struct {
    uint16_t id,part;
    const char *name;
    uint32_t flash_bytes,ram_bytes,clock_hz,gpio_base;
    uint8_t port,pin;
    uint64_t capabilities;
} model_profile;
static const model_profile models[]={
    {1,0xc23,"stm32f103rb",131072,20480,8000000,0x40010800,'A',5,1},
    {2,0xc24,"stm32f407vg",1048576,131072,16000000,0x40020c00,'D',12,7},
    {3,0xc27,"stm32f746ng",1048576,327680,16000000,0x40022000,'I',1,0x3b}
};
typedef struct {
    const model_profile *model;
    uint32_t cpuid,flash_base,flash_bytes,ram_base,ram_bytes,clock_hz,gpio_base;
    uint8_t port,pin;
    uint64_t capabilities;
} device_view;
typedef struct {
    size_t code,metadata;
    unsigned fields;
    ueec_md_stm32_target target;
    uint64_t required;
    ueec_md_stm32_blink blink;
    uint16_t priority;
    uint32_t sp,reset;
} candidate;

static int read_blob(const char *path,uint8_t **data,size_t *size)
{
    int fd=open(path,O_RDONLY);struct stat st;
    if (fd<0) return -1;
    if (fstat(fd,&st)) {close(fd);return -1;}
    if (st.st_size<8 || (uintmax_t)st.st_size>64U*1024U*1024U) {close(fd);errno=EINVAL;return -1;}
    *size=(size_t)st.st_size;*data=malloc(*size);
    int rc=*data ? pread_full(fd,*data,*size,0) : -1;close(fd);return rc;
}

static int read_device(const uint8_t *p,size_t size,device_view *v)
{
    if (size!=48U || memcmp(p,"STP1",4) || read_u16(p+6,0) || read_u16(p+46,0)) return -1;
    uint16_t id=read_u16(p+4,0);memset(v,0,sizeof(*v));
    for (size_t i=0;i<sizeof(models)/sizeof(models[0]);++i) if (models[i].id==id) v->model=&models[i];
    if (!v->model) return -1;
    v->cpuid=read_u32(p+8,0);v->flash_base=read_u32(p+12,0);v->flash_bytes=read_u32(p+16,0);
    v->ram_base=read_u32(p+20,0);v->ram_bytes=read_u32(p+24,0);v->clock_hz=read_u32(p+28,0);
    v->capabilities=read_u64(p+32,0);v->gpio_base=read_u32(p+40,0);v->port=p[44];v->pin=p[45];
    const model_profile *m=v->model;
    if (((v->cpuid>>4)&0xfffU)!=m->part || v->flash_base!=UINT32_C(0x08000000) ||
        v->ram_base!=UINT32_C(0x20000000) || v->flash_bytes!=m->flash_bytes || v->ram_bytes!=m->ram_bytes ||
        v->clock_hz!=m->clock_hz || v->capabilities!=m->capabilities || v->gpio_base!=m->gpio_base ||
        v->port!=m->port || v->pin!=m->pin) return -1;
    return 0;
}

static uint64_t integer(const uint8_t *p,size_t width,unsigned endian)
{
    return width==2U ? read_u16(p,endian) : width==4U ? read_u32(p,endian) : read_u64(p,endian);
}

static int metadata(const uint8_t *p,size_t size,unsigned endian,size_t section,size_t nodes,candidate *all)
{
    if (size<6U || read_u16(p+2,endian)!=UEEC_MD_DESIGN_NODE_INDEXED) return -1;
    unsigned format=read_u16(p,endian);if (format>2U) return -1;
    size_t width=(size_t)2U<<format,head=4U+width,entry=4U+width;
    if (!span_ok(size,4,width)) return -1;
    uint64_t count=integer(p+4,width,endian);
    if (!count || count>=(uint64_t)((size-head)/entry)) return -1;
    size_t data_at=head+((size_t)count+1U)*entry;uint64_t previous=0;
    for (size_t i=0;i<=(size_t)count;++i) {
        const uint8_t *element=p+head+i*entry;
        size_t node=read_u16(element,endian);uint16_t type=read_u16(element+2,endian);
        uint64_t at=integer(element+4,width,endian);
        if (at<previous || at>(uint64_t)(size-data_at) || (!i && at)) return -1;
        previous=at;
        if (i==(size_t)count) {if (node || type || at!=(uint64_t)(size-data_at)) return -1;continue;}
        if (node>=nodes || all[node].metadata!=section) return -1;
        uint64_t end=integer(element+entry+4,width,endian);
        if (end<at || end>(uint64_t)(size-data_at)) return -1;
        const uint8_t *d=p+data_at+(size_t)at;size_t length=(size_t)(end-at);candidate *c=&all[node];unsigned field;
        if (type==UEEC_MD_TYPE_STM32_TARGET && length==24U) {
            field=1;c->target.model_id=read_u16(d,endian);c->target.core_part=read_u16(d+2,endian);
            c->target.flash_base=read_u32(d+4,endian);c->target.flash_bytes=read_u32(d+8,endian);
            c->target.ram_base=read_u32(d+12,endian);c->target.ram_bytes=read_u32(d+16,endian);c->target.clock_hz=read_u32(d+20,endian);
            if (!c->target.model_id || !c->target.core_part || c->target.core_part>0xfffU || !c->target.flash_bytes ||
                !c->target.ram_bytes || !c->target.clock_hz ||
                (uint64_t)c->target.flash_base+c->target.flash_bytes>UINT32_MAX ||
                (uint64_t)c->target.ram_base+c->target.ram_bytes>UINT32_MAX) return -1;
        } else if (type==UEEC_MD_TYPE_STM32_REQUIRED && length==8U) {field=2;c->required=read_u64(d,endian);}
        else if (type==UEEC_MD_TYPE_STM32_BLINK && length==8U) {
            field=4;c->blink.gpio_base=read_u32(d,endian);c->blink.port=d[4];c->blink.pin=d[5];c->blink.half_period_ms=read_u16(d+6,endian);
            if (c->blink.port<'A' || c->blink.port>'I' || c->blink.pin>15U || !c->blink.half_period_ms || c->blink.half_period_ms>1000U) return -1;
        } else if (type==UEEC_MD_TYPE_PRIORITY && length==2U) {field=8;c->priority=read_u16(d,endian);}
        else return -1;
        if (c->fields&field) return -1;
        c->fields|=field;
    }
    return 0;
}

static int collect(const file_view *file,const ueec_view *view,candidate *all)
{
    for (size_t n=0;n<view->nodes;++n) {
        if ((node_word(file,view,n)&UINT32_C(0x07ffffff))!=UINT32_C(0x488) || !node_name_equals(file,view,n,"blink")) return -1;
        all[n].code=all[n].metadata=SIZE_MAX;
    }
    for (size_t i=0;i<view->node_info_count;++i) {
        size_t n=mapping_node(file,view,i),global=mapping_section(file,view,i);section_view s;
        if (get_section(file,view,global,&s)) return -1;
        if (s.type==UEEC_SECTION_CODE && all[n].code==SIZE_MAX && s.data_size>=64U && !(s.data_size&1U)) all[n].code=global;
        else if (s.type==UEEC_SECTION_METADATA && all[n].metadata==SIZE_MAX) all[n].metadata=global;
        else return -1;
    }
    for (size_t i=1;i<view->section_count;++i) {
        section_view s;uint64_t at;
        if (get_section(file,view,i,&s)) return -1;
        if (s.type!=UEEC_SECTION_METADATA) continue;
        if (section_file_offset(file,view,i,&at) || at>(uint64_t)file->size || s.data_size>(uint64_t)file->size-at ||
            metadata(file->data+(size_t)at,(size_t)s.data_size,view->endian,i,view->nodes,all)) return -1;
    }
    for (size_t n=0;n<view->nodes;++n) {
        candidate *c=&all[n];section_view code;uint64_t at;
        if (c->code==SIZE_MAX || c->metadata==SIZE_MAX || (c->fields&7U)!=7U || get_section(file,view,c->code,&code) ||
            code.data_size>c->target.flash_bytes || section_file_offset(file,view,c->code,&at)) return -1;
        /* CODE vectors are always little endian, independently of the container. */
        c->sp=read_u32(file->data+(size_t)at,0);c->reset=read_u32(file->data+(size_t)at+4,0);
        uint64_t ram_end=(uint64_t)c->target.ram_base+c->target.ram_bytes;
        uint64_t code_end=(uint64_t)c->target.flash_base+code.data_size;
        if ((c->sp&7U) || c->sp<=c->target.ram_base || c->sp>ram_end || !(c->reset&1U) ||
            (c->reset&~UINT32_C(1))<c->target.flash_base+64U || (c->reset&~UINT32_C(1))>=code_end) return -1;
    }
    return 0;
}

static int matches(const candidate *c,const device_view *v)
{
    return c->target.model_id==v->model->id && c->target.core_part==((v->cpuid>>4)&0xfffU) &&
        c->target.flash_base==v->flash_base && c->target.flash_bytes==v->flash_bytes &&
        c->target.ram_base==v->ram_base && c->target.ram_bytes==v->ram_bytes && c->target.clock_hz==v->clock_hz &&
        c->blink.gpio_base==v->gpio_base && c->blink.port==v->port && c->blink.pin==v->pin && !(c->required&~v->capabilities);
}

static int write_raw(const char *path,const uint8_t *p,size_t size)
{
    FILE *f=fopen(path,"wbx");if (!f) return -1;
    int rc=fwrite(p,1,size,f)==size ? 0 : -1;if (fclose(f)) rc=-1;
    if (rc) unlink(path);
    return rc;
}

static int select_main(int argc,char **argv)
{
    const char *profile_path=NULL,*path=NULL,*output=NULL,*manifest=NULL;int list=0;
    for (int i=2;i<argc;++i) {
        const char *a=argv[i];
        if (!strcmp(a,"--list-nodes")) list=1;
        else if (!strcmp(a,"--no-run")) continue;
        else if (i+1<argc && !strcmp(a,"--device-profile")) profile_path=argv[++i];
        else if (i+1<argc && !strcmp(a,"--extract")) output=argv[++i];
        else if (i+1<argc && !strcmp(a,"--manifest")) manifest=argv[++i];
        else if (a[0]=='-' || path) return 2;
        else path=a;
    }
    if (!profile_path || !path) return 2;
    uint8_t *profile_data=NULL,*blob=NULL;size_t profile_size=0,size=0;candidate *all=NULL;int rc=1;device_view device;
    if (read_blob(profile_path,&profile_data,&profile_size) || read_device(profile_data,profile_size,&device)) {
        fprintf(stderr,"error: invalid Renode device profile\n");goto done;
    }
    printf("DEVICE_PROFILE model=%s cpuid=0x%08" PRIx32 " clock=%" PRIu32 "\n",device.model->name,device.cpuid,device.clock_hz);
    if (read_blob(path,&blob,&size)) {perror("UEEC");goto done;}
    file_view file={blob,size};ueec_view view;
    if (parse_ueec(&file,&view)) goto done;
    all=calloc(view.nodes,sizeof(*all));
    if (!all || collect(&file,&view,all)) {fprintf(stderr,"error: malformed STM32 CODE/METADATA/vectors\n");goto done;}
    size_t best=SIZE_MAX;
    for (size_t n=0;n<view.nodes;++n) {
        int match=matches(&all[n],&device);
        if (list) printf("node=%zu model_id=%u core_part=0x%x CODE=%zu METADATA=%zu priority=%u compatible=%s\n",
            n,all[n].target.model_id,all[n].target.core_part,all[n].code,all[n].metadata,all[n].priority,match ? "yes" : "no");
        if (match && (best==SIZE_MAX || all[n].priority>all[best].priority)) best=n;
    }
    if (list) {rc=0;goto done;}
    if (best==SIZE_MAX) {fprintf(stderr,"error: no compatible STM32 node\n");goto done;}
    candidate *c=&all[best];section_view code;uint64_t at;
    if (get_section(&file,&view,c->code,&code) || section_file_offset(&file,&view,c->code,&at)) goto done;
    printf("SELECTED model=%s node=%zu CODE=%zu METADATA=%zu priority=%u mode=automatic\n",
           device.model->name,best,c->code,c->metadata,c->priority);
    if (output && write_raw(output,blob+(size_t)at,(size_t)code.data_size)) {perror("CODE (will not overwrite)");goto done;}
    if (manifest) {
        FILE *f=fopen(manifest,"wx");if (!f) {perror("manifest (will not overwrite)");goto done;}
        int status=fprintf(f,"{\"node\":%zu,\"code_bytes\":%" PRIu64 ",\"flash_base\":%" PRIu32 ",\"sp\":%" PRIu32
                          ",\"reset\":%" PRIu32 ",\"gpio_base\":%" PRIu32 ",\"port\":%u,\"pin\":%u,\"half_period_ms\":%u}\n",
                           best,code.data_size,c->target.flash_base,c->sp,c->reset,c->blink.gpio_base,c->blink.port,c->blink.pin,c->blink.half_period_ms);
        if (fclose(f) || status<0) goto done;
    }
    rc=0;
done:
    free(profile_data);free(all);free(blob);return rc;
}

int main(int argc,char **argv)
{
    if (argc>1 && !strcmp(argv[1],"--select")) return select_main(argc,argv);
    /* Native C remains the container parser/selector. Python only bridges the
       external Renode process, whose CPU/GPIO models execute the firmware. */
    char executable[4096],script[4200];ssize_t length=readlink("/proc/self/exe",executable,sizeof(executable)-1U);
    if (length<0 || (size_t)length>=sizeof(executable)-1U) return 2;
    executable[length]=0;char *slash=strrchr(executable,'/');if (!slash) return 2;*slash=0;
    int written=snprintf(script,sizeof(script),"%s/../run_stm32.py",executable);
    if (written<0 || (size_t)written>=sizeof(script)) return 2;
    char **args=calloc((size_t)argc+2U,sizeof(*args));if (!args) return 2;
    args[0]="python3";args[1]=script;for (int i=1;i<argc;++i) args[i+1]=argv[i];
    execvp(args[0],args);perror("Python Renode bridge");free(args);return 2;
}
