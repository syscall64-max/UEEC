#define main ueec_reference_main
#include "reference/ueec_load.c"
#undef main
#include "ueec_avr_metadata.h"
#include <sim_avr.h>
#include <sim_io.h>
#include <avr_ioport.h>

typedef struct {
    const char *name;
    uint8_t signature[3],gcc_arch,sp_bytes;
    uint32_t clock_hz;
    uint64_t capabilities;
} device_profile;

static const device_profile devices[]={
    {"attiny85",{0x1e,0x93,0x0b},25,2,8000000,UINT64_C(0x07)},
    {"atmega328p",{0x1e,0x95,0x0f},5,2,16000000,UINT64_C(0x1f)},
    {"atmega2560",{0x1e,0x98,0x01},6,2,16000000,UINT64_C(0xff)}
};
static const char *isa_names[]={"movw","lpmx","adiw_sbiw","mul","jmp_call","elpm","elpmx","eijmp_eicall"};

typedef struct {
    size_t code_section,metadata_section;
    unsigned fields;
    ueec_md_avr_target target;
    uint64_t required;
    ueec_md_avr_blink blink;
    uint16_t priority;
} avr_candidate;

static uint64_t md_number(const uint8_t *p,size_t width,unsigned endian)
{
    return width==2U ? read_u16(p,endian) : width==4U ? read_u32(p,endian) : read_u64(p,endian);
}

static int parse_avr_metadata(const uint8_t *p,size_t size,unsigned endian,
                              size_t section,size_t nodes,avr_candidate *candidates)
{
    if (size<6U || read_u16(p+2,endian)!=UEEC_MD_DESIGN_NODE_INDEXED) return -1;
    unsigned format=read_u16(p,endian);
    if (format>2U) return -1;
    size_t width=(size_t)2U<<format,header=4U+width,entry_size=4U+width;
    if (!span_ok(size,4,width)) return -1;
    uint64_t count=md_number(p+4,width,endian);
    if (!count || count>=(uint64_t)((size-header)/entry_size)) return -1;
    size_t data_at=header+((size_t)count+1U)*entry_size;
    uint64_t previous=0;
    for (size_t i=0;i<=(size_t)count;++i) {
        const uint8_t *entry=p+header+i*entry_size;
        size_t node=read_u16(entry,endian);uint16_t kind=read_u16(entry+2,endian);
        uint64_t at=md_number(entry+4,width,endian);
        if (at<previous || at>(uint64_t)(size-data_at) || (!i && at)) return -1;
        previous=at;
        if (i==(size_t)count) {
            if (node || kind || at!=(uint64_t)(size-data_at)) return -1;
            continue;
        }
        if (node>=nodes || candidates[node].metadata_section!=section) return -1;
        uint64_t end=md_number(entry+entry_size+4,width,endian);
        if (end<at || end>(uint64_t)(size-data_at)) return -1;
        size_t length=(size_t)(end-at);const uint8_t *data=p+data_at+(size_t)at;
        avr_candidate *c=&candidates[node];unsigned field;
        if (kind==UEEC_MD_TYPE_AVR_TARGET && length==24U) {
            field=1U;memcpy(c->target.signature,data,3);c->target.gcc_arch=data[3];
            c->target.pc_bytes=data[4];c->target.sp_bytes=data[5];c->target.reserved=read_u16(data+6,endian);
            c->target.flash_bytes=read_u32(data+8,endian);c->target.sram_bytes=read_u32(data+12,endian);
            c->target.eeprom_bytes=read_u32(data+16,endian);c->target.clock_hz=read_u32(data+20,endian);
            if (c->target.reserved || !c->target.gcc_arch ||
                (c->target.pc_bytes!=2U && c->target.pc_bytes!=3U) ||
                (c->target.sp_bytes!=1U && c->target.sp_bytes!=2U) ||
                !c->target.flash_bytes || !c->target.sram_bytes || !c->target.eeprom_bytes ||
                !c->target.clock_hz || c->target.clock_hz>40000000U) return -1;
        } else if (kind==UEEC_MD_TYPE_AVR_REQUIRED && length==8U) {
            field=2U;c->required=read_u64(data,endian);
        } else if (kind==UEEC_MD_TYPE_AVR_BLINK && length==4U) {
            field=4U;c->blink.port=data[0];c->blink.pin=data[1];c->blink.half_period_ms=read_u16(data+2,endian);
            if (c->blink.port<'A' || c->blink.port>'L' || c->blink.pin>7U ||
                !c->blink.half_period_ms || c->blink.half_period_ms>1000U) return -1;
        } else if (kind==UEEC_MD_TYPE_PRIORITY && length==2U) {
            field=8U;c->priority=read_u16(data,endian);
        } else return -1;
        if (c->fields&field) return -1;
        c->fields|=field;
    }
    return 0;
}

static int collect_avr_candidates(const file_view *file,const ueec_view *view,avr_candidate *candidates)
{
    for (size_t n=0;n<view->nodes;++n) {
        /* Only standalone, bare-metal AVR8 MPU programs in this example. */
        if ((node_word(file,view,n)&UINT32_C(0x07ffffff))!=UINT32_C(0x88) ||
            !node_name_equals(file,view,n,"blink")) return -1;
        candidates[n].code_section=SIZE_MAX;candidates[n].metadata_section=SIZE_MAX;
    }
    for (size_t i=0;i<view->node_info_count;++i) {
        size_t node=mapping_node(file,view,i),global=mapping_section(file,view,i);section_view section;
        if (get_section(file,view,global,&section)) return -1;
        avr_candidate *c=&candidates[node];
        if (section.type==UEEC_SECTION_CODE && c->code_section==SIZE_MAX && section.data_size>0U && !(section.data_size&1U)) c->code_section=global;
        else if (section.type==UEEC_SECTION_METADATA && c->metadata_section==SIZE_MAX) c->metadata_section=global;
        else return -1;
    }
    for (size_t global=1;global<view->section_count;++global) {
        section_view section;uint64_t offset;
        if (get_section(file,view,global,&section)) return -1;
        if (section.type!=UEEC_SECTION_METADATA) continue;
        if (section_file_offset(file,view,global,&offset) || offset>(uint64_t)file->size ||
            section.data_size>(uint64_t)file->size-offset ||
            parse_avr_metadata(file->data+(size_t)offset,(size_t)section.data_size,view->endian,
                               global,view->nodes,candidates)) return -1;
    }
    for (size_t n=0;n<view->nodes;++n) {
        section_view code;avr_candidate *c=&candidates[n];
        if (c->code_section==SIZE_MAX || c->metadata_section==SIZE_MAX || (c->fields&7U)!=7U ||
            get_section(file,view,c->code_section,&code) || code.data_size>c->target.flash_bytes) return -1;
    }
    return 0;
}

static int compatible(const avr_candidate *c,const device_profile *profile,const avr_t *avr)
{
    return !memcmp(c->target.signature,avr->signature,3) && c->target.gcc_arch==profile->gcc_arch &&
        c->target.pc_bytes==avr->address_size && c->target.sp_bytes==profile->sp_bytes &&
        c->target.flash_bytes==avr->flashend+1U && c->target.sram_bytes==(uint32_t)avr->ramend-(uint32_t)avr->ioend &&
        c->target.eeprom_bytes==avr->e2end+1U && c->target.clock_hz==avr->frequency &&
        !(c->required & ~profile->capabilities);
}

static void print_isa(uint64_t flags)
{
    int first=1;
    for (unsigned i=0;i<8U;++i) if (flags&(UINT64_C(1)<<i)) {
        printf("%s%s",first ? "" : ",",isa_names[i]);first=0;
    }
    if (flags&~UEEC_AVR_KNOWN_MASK) {printf("%sunknown",first ? "" : ",");first=0;}
    if (first) printf("baseline");
}

typedef struct {
    avr_t *avr;
    ueec_md_avr_blink blink;
    FILE *vcd;
    unsigned edges;
    int level,bad_timing;
    uint64_t previous;
} gpio_watch;

static void gpio_changed(struct avr_irq_t *irq,uint32_t value,void *opaque)
{
    (void)irq;gpio_watch *w=opaque;avr_ioport_state_t state;
    if (avr_ioctl(w->avr,AVR_IOCTL_IOPORT_GETSTATE(w->blink.port),&state) ||
        !(state.ddr&(1UL<<w->blink.pin))) return;
    int level=(int)(value&1U);
    if (w->level==level) return;
    uint64_t cycle=(uint64_t)w->avr->cycle;
    if (w->level>=0) {
        if (w->edges) {
            uint64_t expected=(uint64_t)w->avr->frequency*w->blink.half_period_ms/1000U;
            uint64_t delta=cycle-w->previous,tolerance=expected/100U+32U;
            if (delta+tolerance<expected || delta>expected+tolerance) w->bad_timing=1;
        }
        ++w->edges;w->previous=cycle;
        printf("GPIO edge=%u port=%c pin=%u level=%d cycle=%" PRIu64 " time_us=%" PRIu64 " LED=%s\n",
               w->edges,(int)w->blink.port,(unsigned)w->blink.pin,level,cycle,
               cycle*UINT64_C(1000000)/w->avr->frequency,level ? "ON" : "OFF");
    }
    w->level=level;
    if (w->vcd) fprintf(w->vcd,"#%" PRIu64 "\n%d!\n",cycle*UINT64_C(1000000000)/w->avr->frequency,level);
}

static int read_blob(const char *path,uint8_t **data,size_t *size)
{
    int fd=open(path,O_RDONLY);struct stat st;
    if (fd<0) return -1;
    if (fstat(fd,&st)) {close(fd);return -1;}
    if (st.st_size<8 || (uintmax_t)st.st_size>64U*1024U*1024U) {close(fd);errno=EINVAL;return -1;}
    *size=(size_t)st.st_size;*data=malloc(*size);
    int rc=*data ? pread_full(fd,*data,*size,0) : -1;
    close(fd);return rc;
}

static int write_exclusive(const char *path,const uint8_t *data,size_t size)
{
    FILE *f=fopen(path,"wbx");if (!f) return -1;
    int rc=fwrite(data,1,size,f)==size ? 0 : -1;
    if (fclose(f)) rc=-1;
    if (rc) unlink(path);
    return rc;
}

static int number(const char *s,uint64_t max,uint64_t *out)
{
    if (!*s || *s=='-' || *s=='+') return -1;
    char *end;errno=0;uintmax_t n=strtoumax(s,&end,10);
    if (errno || *end || n>max) return -1;
    *out=(uint64_t)n;return 0;
}

static void avr_usage(const char *app)
{
    printf("Usage: %s [--mcu MODEL] [options] FILE.ueec\n"
           "  --list-devices       show the three supported device profiles\n"
           "  --list-nodes         inspect each node and its metadata\n"
           "  --groups             display target ISA groups\n"
           "  --no-run             select without writing virtual Flash\n"
           "  --frequency HZ       configure the virtual MCU clock\n"
           "  --edges N            verify N output transitions (default 8)\n"
           "  --max-cycles N       simulation budget (test override)\n"
           "  --vcd FILE           record real GPIO transitions; no overwrite\n"
           "  --extract FILE       save only selected raw Flash CODE; no overwrite\n"
           "Default virtual device: atmega328p. No physical device is programmed.\n",app);
}

int main(int argc,char **argv)
{
    const char *mcu="atmega328p",*path=NULL,*vcd_path=NULL,*extract=NULL;
    uint64_t frequency=0,edge_count=8,max_cycles=0;int list=0,no_run=0,groups=0;
    for (int i=1;i<argc;++i) {
        const char *a=argv[i];
        if (!strcmp(a,"--help")) {avr_usage(argv[0]);return 0;}
        if (!strcmp(a,"--list-devices")) {
            for (size_t j=0;j<sizeof(devices)/sizeof(devices[0]);++j)
                printf("%s signature=%02x%02x%02x core=avr%u clock=%" PRIu32 "\n",devices[j].name,
                    devices[j].signature[0],devices[j].signature[1],devices[j].signature[2],devices[j].gcc_arch,devices[j].clock_hz);
            return 0;
        }
        if (!strcmp(a,"--list-nodes")) list=1;
        else if (!strcmp(a,"--no-run")) no_run=1;
        else if (!strcmp(a,"--groups")) groups=1;
        else if (i+1<argc && !strcmp(a,"--mcu")) mcu=argv[++i];
        else if (i+1<argc && !strcmp(a,"--vcd")) vcd_path=argv[++i];
        else if (i+1<argc && !strcmp(a,"--extract")) extract=argv[++i];
        else if (i+1<argc && !strcmp(a,"--frequency")) {if (number(argv[++i],40000000,&frequency) || !frequency) return 2;}
        else if (i+1<argc && !strcmp(a,"--edges")) {if (number(argv[++i],64,&edge_count) || edge_count<3U) return 2;}
        else if (i+1<argc && !strcmp(a,"--max-cycles")) {if (number(argv[++i],1000000000,&max_cycles) || !max_cycles) return 2;}
        else if (a[0]=='-' || path) {avr_usage(argv[0]);return 2;}
        else path=a;
    }
    const device_profile *profile=NULL;
    for (size_t i=0;i<sizeof(devices)/sizeof(devices[0]);++i) if (!strcmp(devices[i].name,mcu)) profile=&devices[i];
    if (!profile) {fprintf(stderr,"error: unsupported virtual device profile\n");return 2;}
    avr_t *avr=avr_make_mcu_by_name(mcu);
    if (!avr || avr_init(avr)) {fprintf(stderr,"error: simavr cannot initialize device\n");return 1;}
    avr->frequency=frequency ? (uint32_t)frequency : profile->clock_hz;
    avr->log=LOG_ERROR;
    if (memcmp(avr->signature,profile->signature,3)) {fprintf(stderr,"error: simulator signature/profile mismatch\n");avr_terminate(avr);return 1;}
    printf("DEVICE model=%s simulator_core=%s signature=%02x%02x%02x core=avr%u clock=%" PRIu32 " Flash=%" PRIu32 " SRAM=%u EEPROM=%" PRIu32 " PC_bytes=%u\n",
        mcu,avr->mmcu,avr->signature[0],avr->signature[1],avr->signature[2],profile->gcc_arch,avr->frequency,
        avr->flashend+1U,(unsigned)(avr->ramend-avr->ioend),avr->e2end+1U,(unsigned)avr->address_size);
    if (groups) {printf("ISA available: ");print_isa(profile->capabilities);putchar('\n');}
    if (!path) {avr_terminate(avr);return groups ? 0 : 2;}
    uint8_t *blob=NULL;size_t size=0;avr_candidate *candidates=NULL;FILE *vcd=NULL;int rc=1;
    if (read_blob(path,&blob,&size)) {perror("read UEEC");goto done;}
    file_view file={blob,size};ueec_view view;
    if (parse_ueec(&file,&view)) goto done;
    candidates=calloc(view.nodes,sizeof(*candidates));
    if (!candidates || collect_avr_candidates(&file,&view,candidates)) {fprintf(stderr,"error: malformed AVR CODE/METADATA\n");goto done;}
    size_t best=SIZE_MAX;
    for (size_t i=0;i<view.nodes;++i) {
        int match=compatible(&candidates[i],profile,avr);
        if (list) {
            const avr_candidate *c=&candidates[i];
            printf("node=%zu signature=%02x%02x%02x core=avr%u CODE=%zu METADATA=%zu priority=%u compatible=%s required=",
                i,c->target.signature[0],c->target.signature[1],c->target.signature[2],c->target.gcc_arch,
                c->code_section,c->metadata_section,(unsigned)c->priority,match ? "yes" : "no");
            print_isa(c->required);printf(" GPIO=P%c%u half_period_ms=%u\n",c->blink.port,c->blink.pin,c->blink.half_period_ms);
        }
        if (match && (best==SIZE_MAX || candidates[i].priority>candidates[best].priority)) best=i;
    }
    if (list) {rc=0;goto done;}
    if (best==SIZE_MAX) {fprintf(stderr,"error: no compatible AVR node for device signature/clock/ISA\n");goto done;}
    avr_candidate selected=candidates[best];section_view code;uint64_t offset;
    if (get_section(&file,&view,selected.code_section,&code) || section_file_offset(&file,&view,selected.code_section,&offset)) goto done;
    printf("SELECTED model=%s node=%zu CODE=%zu METADATA=%zu priority=%u mode=automatic\n",mcu,best,
           selected.code_section,selected.metadata_section,(unsigned)selected.priority);
    if (extract && write_exclusive(extract,blob+(size_t)offset,(size_t)code.data_size)) {perror("extract (will not overwrite)");goto done;}
    if (no_run) {rc=0;goto done;}
    avr_irq_t *pin=avr_io_getirq(avr,AVR_IOCTL_IOPORT_GETIRQ(selected.blink.port),selected.blink.pin);
    if (!pin) {fprintf(stderr,"error: GPIO pin not available\n");goto done;}
    if (vcd_path) {
        vcd=fopen(vcd_path,"wx");if (!vcd) {perror("VCD (will not overwrite)");goto done;}
        fprintf(vcd,"$timescale 1ns $end\n$scope module ueec_avr $end\n$var wire 1 ! LED $end\n$upscope $end\n$enddefinitions $end\n$dumpvars\nx!\n$end\n");
    }
    avr_loadcode(avr,blob+(size_t)offset,(uint32_t)code.data_size,0);
    if (memcmp(avr->flash,blob+(size_t)offset,(size_t)code.data_size)) {fprintf(stderr,"error: virtual Flash verification failed\n");goto done;}
    avr->codeend=(uint32_t)code.data_size;
    printf("FLASH bytes=%" PRIu64 " address=0 verified=yes metadata_written=no\n",code.data_size);
    free(candidates);candidates=NULL;free(blob);blob=NULL;
    gpio_watch watch={.avr=avr,.blink=selected.blink,.vcd=vcd,.level=-1};
    avr_irq_register_notify(pin,gpio_changed,&watch);
    uint64_t budget=max_cycles ? max_cycles : (uint64_t)avr->frequency*selected.blink.half_period_ms*(edge_count+2U)/1000U+avr->frequency/10U;
    while (watch.edges<edge_count && (uint64_t)avr->cycle<budget && !watch.bad_timing) {
        int state=avr_run(avr);
        if (state==cpu_Crashed || state==cpu_Done) break;
    }
    avr_irq_unregister_notify(pin,gpio_changed,&watch);
    if (watch.edges>=edge_count && !watch.bad_timing) {
        printf("PASS: virtual Flash + AVR execution + %u GPIO transitions; half_period_ms=%u; timing_tolerance=1%%\n",
               watch.edges,(unsigned)selected.blink.half_period_ms);rc=0;
    } else fprintf(stderr,"FAIL: AVR simulation edges=%u expected=%" PRIu64 " timing_error=%d state=%d cycles=%" PRIu64 "\n",
                    watch.edges,edge_count,watch.bad_timing,avr->state,(uint64_t)avr->cycle);
done:
    if (vcd && fclose(vcd)) rc=1;
    free(candidates);free(blob);avr_terminate(avr);return rc;
}
