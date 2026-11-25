#ifndef UEEC_CPU_H
#define UEEC_CPU_H
#include <sys/auxv.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#if defined(__riscv)
#include <sys/wait.h>
#include <sys/resource.h>
/* Some user-mode emulators advertise V via auxv but lack V_GET_CONTROL.
   Probe only in a child, inheriting the calling thread's vector policy.
   A failed instruction cannot kill the loader or cause an unsafe selection. */
static int rvv_child_probe(void)
{
    pid_t child=fork();
    if (child<0) return 0;
    if (child==0) {
        struct rlimit no_core={0,0};
        (void)setrlimit(RLIMIT_CORE,&no_core);
        __asm__ volatile(".option push\n.option arch,+v\n"
                         "vsetvli zero,zero,e32,m1,ta,ma\n.option pop" : : : "memory");
        _exit(0);
    }
    int status;
    while (waitpid(child,&status,0)<0) if (errno!=EINTR) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
}
#endif
#include "ueec_capabilities.h"
#if defined(__i386__) || defined(__x86_64__)
#include "cpu_x86.h"
#endif

typedef struct {
    unsigned machine;
    uint64_t hardware, usable;
    int hardware_known;
    char description[180];
    const char *source;
} dispatch_cpu;

static dispatch_cpu dispatch_detect_cpu(void)
{
    dispatch_cpu c;
    memset(&c,0,sizeof(c));
    c.machine=(unsigned)host_machine();
    c.source="Linux AT_HWCAP/AT_HWCAP2 (OS-advertised capabilities)";
    snprintf(c.description,sizeof(c.description),"%s",family_for(c.machine).architecture);
#if defined(__i386__) || defined(__x86_64__)
    cpu_features x=detect_cpu();
    c.hardware=x.hardware;c.usable=x.usable;c.hardware_known=1;
    snprintf(c.description,sizeof(c.description),"%s | %s",x.vendor,x.brand);
    c.source="CPUID + XSAVE/OSXSAVE + XGETBV; AMX execution disabled";
    /* Kept for source compatibility with example 11. */
    (void)print_features;
#else
    unsigned long h=getauxval(AT_HWCAP),h2=getauxval(AT_HWCAP2);
    (void)h2;
#define CAP(bit,word,mask) do { if ((word)&(mask)) c.usable|=UINT64_C(1)<<(bit); } while (0)
#if defined(__arm__)
    const unsigned pos[]={12,6,13,16,17,18};
    for (unsigned i=0;i<6U;++i) CAP(i,h,1UL<<pos[i]);
    for (unsigned i=0;i<5U;++i) CAP(i+6U,h2,1UL<<i);
#elif defined(__aarch64__)
    const unsigned pos[]={1,22,0,3,4,5,6,7,8};
    for (unsigned i=0;i<9U;++i) CAP(i,h,1UL<<pos[i]);
    CAP(9,h2,1UL<<1);CAP(10,h,1UL<<17);CAP(11,h,1UL<<21);
    CAP(12,h,1UL<<20);CAP(13,h2,1UL<<13);CAP(14,h2,1UL<<14);
    if ((c.usable & 2U) && prctl(51,0UL,0UL,0UL,0UL)<0) c.usable &= ~UINT64_C(2);
    if (!(c.usable&2U)) c.usable &= ~(UINT64_C(1)<<9);
#elif defined(__loongarch__)
    const unsigned pos[]={4,5,3,2,0};
    for (unsigned i=0;i<5U;++i) CAP(i,h,1UL<<pos[i]);
#elif defined(__powerpc__)
    CAP(0,h,0x10000000UL);CAP(1,h,0x80UL);CAP(2,h,0x100UL);
    CAP(3,h2,0x80000000UL);CAP(4,h2,0x00800000UL);CAP(5,h2,0x00040000UL);
    CAP(6,h,0x400UL);CAP(7,h2,0x02000000UL);CAP(8,h2,0x40000000UL);
#elif defined(__riscv)
    const unsigned pos[]={21,8,12,0,5,3,2};
    for (unsigned i=0;i<7U;++i) CAP(i,h,1UL<<pos[i]);
    /* Linux per-thread V policy is authoritative; never enable it here. */
    if (c.usable&1U) {
        int control=prctl(70,0UL,0UL,0UL,0UL); /* PR_RISCV_V_GET_CONTROL */
        int enabled=control>=0 ? (control&3)==2 :
            ((errno==EINVAL || errno==ENOSYS) && rvv_child_probe());
        if (!enabled) c.usable &= ~UINT64_C(1);
    }
    struct {int64_t key;uint64_t value;} probe={4,0};
    /* __NR_riscv_hwprobe=258 on Linux RISC-V; unknown keys return key=-1. */
    if (syscall(258,&probe,(size_t)1,(size_t)0,NULL,0U)==0 && probe.key==4) {
        CAP(7,probe.value,UINT64_C(1)<<3);CAP(8,probe.value,UINT64_C(1)<<4);
        CAP(9,probe.value,UINT64_C(1)<<5);
    }
#elif defined(__sparc__)
    CAP(0,h,0x2000UL);CAP(1,h,0x4000UL);CAP(2,h,0x20000UL);
    CAP(3,h,0x1000UL);CAP(4,h,0x10000UL);
#elif defined(__s390x__)
    CAP(0,h,1UL<<11);CAP(1,h,1UL<<13);CAP(2,h,1UL<<15);
    CAP(3,h,1UL<<3);CAP(4,h,1UL<<2);CAP(5,h,1UL<<6);
#endif
#undef CAP
#endif
    return c;
}
#endif
