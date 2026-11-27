/* Freestanding raw-code test ABI, not a general ELF loader. */
typedef __UINTPTR_TYPE__ word;
typedef int (*binary_fn)(int,int);
#if defined(DATA)
int payload(const word *slots, void *a, void *b) {
 (void)a;(void)b;
 const unsigned char *ro=(const unsigned char *)slots[0];
 unsigned char *rw=(unsigned char *)slots[8/sizeof(word)];
 unsigned char *bss=(unsigned char *)slots[16/sizeof(word)];
 *rw+=10; *bss+=42; return *ro+*rw+*bss;
}
#elif defined(APP)
static __attribute__((always_inline)) inline int invoke(word addr,int a,int b) {
#if defined(__powerpc64__) && (!defined(_CALL_ELF) || _CALL_ELF == 1)
#error PPC64 ELFv1 must use payload_ppc64.S
#else
    return ((binary_fn)addr)(a,b);
#endif
}
int payload(const word *slots, void *unused1, void *unused2) {
    (void)unused1; (void)unused2;
    int a=invoke(slots[0],20,22);
    int b=invoke(slots[8/sizeof(word)],7,5);
    int c=invoke(slots[16/sizeof(word)],6,7);
    return a+b+c;
}
#elif defined(ADD)
int payload(int a,int b){return a+b;}
#else
int payload(int a,int b){return a*b;}
#endif
