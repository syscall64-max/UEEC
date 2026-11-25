/* The benchmark calls the exact stage-16 parser/allocation implementation.
 * Rename its CLI entry point inside this translation unit; the guest entry
 * point and the interpreter are never called by benchmark main below.
 * No copied parser or changes to the production loader are needed. */
#define main ueec_vm_cli_main
#include "../16_ueec_vm/ueec_vm.c"
#undef main

#include <time.h>

#if defined(CLOCK_MONOTONIC_RAW)
# define BENCH_CLOCK CLOCK_MONOTONIC_RAW
# define BENCH_CLOCK_NAME "CLOCK_MONOTONIC_RAW"
#else
# define BENCH_CLOCK CLOCK_MONOTONIC
# define BENCH_CLOCK_NAME "CLOCK_MONOTONIC"
#endif

typedef struct {
    uint64_t stamp[6];
    uint64_t mapped_bytes, selected_data_bytes;
    size_t mapped_regions;
} bench_sample;

static uint64_t bench_now(void)
{
    struct timespec t;
    if (clock_gettime(BENCH_CLOCK,&t)!=0) {
        perror("clock_gettime"); exit(2);
    }
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}

static int bench_one(const char *path,const char *selector,load_mode mode,bench_sample *sample)
{
    file_view file={0}; ueec_view view; loaded_image image;
    struct stat st; size_t selected;
    sample->stamp[0]=bench_now();
    int fd=open(path,O_RDONLY);
    if (fd<0) { perror("open"); return 1; }
    if (fstat(fd,&st)!=0 || st.st_size<=0 || (uintmax_t)st.st_size>(uintmax_t)SIZE_MAX) {
        fprintf(stderr,"invalid input size\n"); (void)close(fd); return 1;
    }
    void *mapping=mmap(NULL,(size_t)st.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    if (mapping==MAP_FAILED) { perror("mmap"); (void)close(fd); return 1; }
    file.data=mapping; file.size=(size_t)st.st_size;
    sample->stamp[1]=bench_now();
    if (parse_ueec(&file,&view) || select_node(&file,&view,selector,&selected)) {
        (void)munmap(mapping,file.size); (void)close(fd); return 1;
    }
    sample->stamp[2]=bench_now();
    if (build_runtime_image(fd,&file,&view,selected,mode,&image)) {
        (void)munmap(mapping,file.size); (void)close(fd); return 1;
    }
    sample->stamp[3]=bench_now();
    int unmap_result=munmap(mapping,file.size);
    int close_result=close(fd);
    if (unmap_result!=0 || close_result!=0) {
        perror("input close"); release_image(&image); return 1;
    }
    sample->stamp[4]=bench_now();
    /* Save counters outside the measured release interval. */
    sample->mapped_bytes=0; sample->selected_data_bytes=0;
    sample->mapped_regions=image.region_count;
    int mode_matches=(image.actual_mode==mode);
    for (size_t i=0;i<image.local_count;++i) {
        sample->mapped_bytes+=(uint64_t)image.allocation_size[i];
        sample->selected_data_bytes+=image.section[i].data_size;
    }
    /* HEADER data_size above refers to the source HEADER; report payload
     * separately so input metadata growth does not change selected payload. */
    sample->selected_data_bytes-=image.section[0].data_size;
    uint64_t release_start=bench_now();
    release_image(&image);
    sample->stamp[5]=bench_now();
    /* Shift release duration to follow stamp[4], excluding counter work. */
    sample->stamp[5]=sample->stamp[4]+(sample->stamp[5]-release_start);
    if (!mode_matches) { fprintf(stderr,"allocation mode changed\n"); return 1; }
    return 0;
}

static int bench_count(const char *s,size_t *out,int allow_zero)
{
    char *end; errno=0; unsigned long n=strtoul(s,&end,10);
    if (errno || end==s || *end || s[0]=='-' || n>100000 || (!allow_zero && !n)) return -1;
    *out=(size_t)n; return 0;
}

int main(int argc,char **argv)
{
    if (argc==2 && strcmp(argv[1],"--info")==0) {
        struct timespec r;
        if (clock_getres(BENCH_CLOCK,&r)) return 2;
        printf("{\"clock\":\"%s\",\"resolution_ns\":%" PRIu64 ",\"compiler\":\"%s\"}\n",
               BENCH_CLOCK_NAME,(uint64_t)r.tv_sec*UINT64_C(1000000000)+(uint64_t)r.tv_nsec,__VERSION__);
        return 0;
    }
    size_t repeats,warmups; load_mode mode;
    if ((argc!=5 && argc!=6) || parse_mode(argv[2],&mode) || mode==LOAD_AUTO ||
        bench_count(argv[3],&repeats,0) || bench_count(argv[4],&warmups,1)) {
        fprintf(stderr,"usage: %s FILE MODE REPEATS WARMUPS [NODE]\n",argv[0]); return 2;
    }
    bench_sample *samples=calloc(repeats,sizeof(*samples));
    if (!samples) return 2;
    const char *selector=argc==6?argv[5]:"0";
    for (size_t i=0;i<warmups+repeats;++i) {
        bench_sample scratch;
        if (bench_one(argv[1],selector,mode,i<warmups?&scratch:&samples[i-warmups])) {
            free(samples); return 1;
        }
    }
    /* Printing and process startup are outside all measured intervals. */
    printf("sample,open_map_ns,parse_select_ns,load_ns,input_close_ns,release_ns,total_ns,mapped_bytes,mapped_regions,selected_payload_bytes\n");
    for (size_t i=0;i<repeats;++i) {
        const bench_sample *s=&samples[i];
        printf("%zu",i);
        for (size_t j=1;j<6;++j) printf(",%" PRIu64,s->stamp[j]-s->stamp[j-1]);
        printf(",%" PRIu64 ",%" PRIu64 ",%zu,%" PRIu64 "\n",
               s->stamp[5]-s->stamp[0],s->mapped_bytes,s->mapped_regions,s->selected_data_bytes);
    }
    free(samples); return 0;
}
