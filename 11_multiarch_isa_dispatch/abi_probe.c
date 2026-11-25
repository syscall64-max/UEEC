/* Run before trying an older emulated CPU. Same compiler/libc as the loader,
   with no UEEC parser, CPU feature probe or raw machine-code call. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc,char **argv)
{
    const char *message=argc>1 ? argv[1] : "libc-baseline";
    size_t n=strlen(message)+1U;
    char *copy=malloc(n);
    if (!copy) return 1;
    memcpy(copy,message,n);
    long page=sysconf(_SC_PAGESIZE);
    if (page<=0) {free(copy);return 1;}
    void *mapping=mmap(NULL,(size_t)page,PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if (mapping==MAP_FAILED) {free(copy);return 1;}
    printf("UEEC ABI baseline probe PASS: %s; HWCAP=%lu\n",copy,getauxval(AT_HWCAP));
    int failed=munmap(mapping,(size_t)page)!=0;
    free(copy);
    return failed;
}
