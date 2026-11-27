//
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
//
static void call_entry(void *address) {
#if defined(__powerpc64__) && defined(_CALL_ELF) && (_CALL_ELF == 1)
    /*
     * PPC64 ELFv1 function pointers address a function descriptor, not the
     * first instruction.  The mapped file contains raw instructions, so jump
     * to its address through CTR instead of treating it as a C function
     * pointer.
     */
    __asm__ volatile(
        "mtctr %0\n\t"
        "bctrl"
        :
        : "r"(address)
        : "ctr", "lr", "memory"
    );
#else
    void (*entry)(void) = (void (*)(void))address;
    entry();
#endif
}
//
int main(int arg, char* argv[]) {
    //
    if(arg != 2) {
        fprintf(stderr, "გასაცემული პარამეტრების რაოდენობა არასწორია");
        return 1;
    }
    //
    int fd = open(argv[1], O_RDONLY);
    if(fd == -1) {
        perror("error open file");
        return 2;
    }
    //
    struct stat st_buf;
    if(fstat(fd, &st_buf) == -1) {
        perror("error get file metadata");
        close(fd);
        return 3;
    }
    //
    void *p_map = mmap(0, st_buf.st_size, PROT_EXEC | PROT_READ, MAP_PRIVATE, fd, 0);
    if(p_map == MAP_FAILED) {
        perror("error map sell code");
        close(fd);
        return 4;
    }
    //
    close(fd);
    //
    call_entry(p_map);
    //
    munmap(p_map, st_buf.st_size);
    //
    return 0;
}
