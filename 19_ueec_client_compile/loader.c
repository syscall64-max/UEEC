/* Offline reader for a single Linux UEEC program with CODE and optional metadata. */
#include "../18_ueec_native/linux_runtime.h"

#ifndef UEEC_CPU_MACHINE
#error "Build with the UEEC machine ID for the target Linux ABI"
#endif

enum {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    CODE_ENCODE=1,
#else
    CODE_ENCODE=0,
#endif
    USER_PROGRAM=(1 << 2) | (1 << 3) | (1 << 5),
    LINUX_ABI=(2 << 18) | (2 << 20),
    NODE_WORD=CODE_ENCODE | USER_PROGRAM | (UEEC_CPU_MACHINE << 10) |
              LINUX_ABI | (1 << 27)
};

static void check_requirements(NativeSections sections)
{
    if (!sections.metadata) return;
#if defined(__x86_64__)
    const Byte *metadata = sections.metadata;
    enum { SSE2 = 1 << 6, AVX = 1 << 12, AVX2 = 1 << 13 };
    REQUIRE(sections.metadata_size == 28 &&
            le16(metadata) == 0 && le16(metadata + 2) == 0 &&
            le16(metadata + 4) == 2 &&
            le16(metadata + 6) == 1 && le16(metadata + 8) == 0 &&
            le16(metadata + 10) == 2 && le16(metadata + 12) == 8 &&
            le16(metadata + 14) == 0 && le16(metadata + 16) == 10 &&
            le32(metadata + 22) == 0,
            "invalid UEEC x86 feature metadata\n");
    U32 required = le32(metadata + 18);
    REQUIRE(required == SSE2 || required == (AVX | AVX2),
            "unsupported UEEC x86 feature mask\n");
    U32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(0), "2"(0));
    U32 max_leaf = a;
    REQUIRE(max_leaf >= 1, "CPUID feature information unavailable\n");
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(1), "2"(0));
    if (required == SSE2) {
        REQUIRE(d & (1U << 26), "SSE2 unavailable\n");
        return;
    }
    U32 avx_state = (1U << 26) | (1U << 27) | (1U << 28);
    REQUIRE(max_leaf >= 7 && (c & avx_state) == avx_state, "AVX2 unavailable\n");
    __asm__ volatile("xgetbv" : "=a"(a), "=d"(d) : "c"(0));
    REQUIRE((a & 6) == 6, "AVX state unavailable\n");
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "0"(7), "2"(0));
    REQUIRE(b & (1U << 5), "AVX2 unavailable\n");
#else
    REQUIRE(0, "unexpected UEEC CPU feature metadata\n");
#endif
}

void run(Size argc, const char **argv) __attribute__((noreturn));
void run(Size argc, const char **argv)
{
    REQUIRE(argc == 3, "usage: load-native FILE_SIZE READ_ONLY_FD\n");
    Size file_size = parse_decimal(argv[1], 3 * 65535);
    Size fd = parse_decimal(argv[2], 1048575);
    REQUIRE(file_size > 0, "invalid file size\n");
    Byte *file = map(file_size, PROT_R, MAP_PRIVATE, fd);
    NativeSections sections = read_sections(file, file_size, NODE_WORD, 1);
    check_requirements(sections);
    Size code_size = sections.code_size;
    Byte *code = map(code_size, PROT_R | PROT_W, MAP_PRIVATE | MAP_ANONYMOUS, (Word)-1);
    for (Size i = 0; i < code_size; ++i) code[i] = sections.code[i];

    sync_code(code, code_size);
    REQUIRE(sys3(SYS_PROTECT, (Word)code, code_size, PROT_R | PROT_X) == 0,
            "mprotect RX failed\n");
    release(file, file_size);
    SAY("offline native UEEC CODE is RX\n");
    enter_code(code);
}
