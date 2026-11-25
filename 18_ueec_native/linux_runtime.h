#ifndef UEEC_EXPERIMENT_LINUX_RUNTIME_H
#define UEEC_EXPERIMENT_LINUX_RUNTIME_H

typedef __UINT8_TYPE__ Byte;
typedef __UINT16_TYPE__ U16;
typedef __UINT32_TYPE__ U32;
typedef __UINTPTR_TYPE__ Word;
typedef __INTPTR_TYPE__ SWord;
typedef __SIZE_TYPE__ Size;

extern SWord linux_call(Word, Word, Word, Word, Word, Word, Word);
extern void enter_code(const Byte *) __attribute__((noreturn));

#if defined(__x86_64__)
enum { SYS_WRITE=1, SYS_MAP=9, SYS_PROTECT=10, SYS_UNMAP=11, SYS_CHECK=27, SYS_EXIT=60 };
#elif defined(__i386__)
enum { SYS_WRITE=4, SYS_MAP=192, SYS_PROTECT=125, SYS_UNMAP=91, SYS_CHECK=218, SYS_EXIT=1 };
#elif defined(__s390x__)
enum { SYS_WRITE=4, SYS_MAP=90, SYS_PROTECT=125, SYS_UNMAP=91, SYS_CHECK=218, SYS_EXIT=1 };
#elif defined(__arm__)
enum { SYS_WRITE=4, SYS_MAP=192, SYS_PROTECT=125, SYS_UNMAP=91, SYS_CHECK=219, SYS_EXIT=1 };
#elif defined(__powerpc__)
#if defined(__powerpc64__)
#define PPC_MAP 90
#else
#define PPC_MAP 192
#endif
enum { SYS_WRITE=4, SYS_MAP=PPC_MAP, SYS_PROTECT=125, SYS_UNMAP=91, SYS_CHECK=206, SYS_EXIT=1 };
#elif defined(__sparc__)
#if __SIZEOF_POINTER__ == 8
#define SPARC_MAP 71
#else
#define SPARC_MAP 56
#endif
enum { SYS_WRITE=4, SYS_MAP=SPARC_MAP, SYS_PROTECT=74, SYS_UNMAP=73, SYS_CHECK=78, SYS_EXIT=1 };
#else
enum { SYS_WRITE=64, SYS_MAP=222, SYS_PROTECT=226, SYS_UNMAP=215, SYS_CHECK=232, SYS_EXIT=93 };
#endif

enum {
    PROT_R=1, PROT_W=2, PROT_X=4, MAP_PRIVATE=2, MAP_ANONYMOUS=32,
    MAX_NATIVE=8192
};

static SWord sys3(Word number, Word a, Word b, Word c)
{
    return linux_call(number, a, b, c, 0, 0, 0);
}

static void say(const char *s, Size length)
{
    (void)sys3(SYS_WRITE, 2, (Word)s, length);
}

#define SAY(s) say(s, sizeof(s) - 1)
#define REQUIRE(test, reason) do { if (!(test)) die(reason, sizeof(reason) - 1); } while (0)

static void die(const char *reason, Size length) __attribute__((noreturn));
static void die(const char *reason, Size length)
{
    say(reason, length);
    (void)sys3(SYS_EXIT, 126, 0, 0);
    __builtin_trap();
}

static Byte *map(Size length, Word protection, Word flags, Word fd)
{
#if defined(__s390x__)
    /* Linux s390x mmap takes a pointer to six machine-word arguments. */
    Word args[] = {0, length, protection, flags, fd, 0};
    SWord address = sys3(SYS_MAP, (Word)args, 0, 0);
#else
    SWord address = linux_call(SYS_MAP, 0, length, protection, flags, fd, 0);
#endif
    REQUIRE((Word)address < (Word)-4095, "mmap failed\n");
    return (Byte *)(Word)address;
}

static void release(Byte *address, Size length)
{
    /* Check the old address immediately, before any operation can reuse it. */
    Byte residency = 0;
    REQUIRE(sys3(SYS_UNMAP, (Word)address, length, 0) == 0, "munmap failed\n");
    REQUIRE(sys3(SYS_CHECK, (Word)address, 1, (Word)&residency) == -12,
            "old mapping still present or mincore unavailable\n");
}

static U16 le16(const Byte *p)
{
    return (U16)(p[0] | ((unsigned)p[1] << 8));
}

static U32 le32(const Byte *p)
{
    return le16(p) | ((U32)le16(p + 2) << 16);
}

static void sync_code(Byte *p, Size size)
{
#if defined(__s390x__)
    __asm__ volatile("bcr 15,0" ::: "memory");
    (void)p; (void)size;
#elif defined(__arm__)
    REQUIRE(sys3(0x0f0002, (Word)p, (Word)(p + size), 0) == 0, "ARM cacheflush failed\n");
#elif defined(__aarch64__)
    for (Size i = 0; i < size; i += 4) __asm__ volatile("dc cvau, %0" :: "r"(p + i) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    for (Size i = 0; i < size; i += 4) __asm__ volatile("ic ivau, %0" :: "r"(p + i) : "memory");
    __asm__ volatile("dsb ish; isb" ::: "memory");
#elif defined(__riscv)
    REQUIRE(sys3(259, (Word)p, (Word)(p + size), 0) == 0, "RISC-V cacheflush failed\n");
#elif defined(__loongarch__)
    __asm__ volatile("ibar 0" ::: "memory");
    (void)p; (void)size;
#elif defined(__powerpc__)
    for (Size i = 0; i < size; i += 4) __asm__ volatile("dcbst 0,%0" :: "r"(p + i) : "memory");
    __asm__ volatile("sync" ::: "memory");
    for (Size i = 0; i < size; i += 4) __asm__ volatile("icbi 0,%0" :: "r"(p + i) : "memory");
    __asm__ volatile("sync; isync" ::: "memory");
#elif defined(__sparc__)
    for (Size i = 0; i < size; i += 4) __asm__ volatile("flush %0" :: "r"(p + i) : "memory");
#else
    (void)p; (void)size;
#endif
}

typedef struct {
    const Byte *code;
    Size code_size;
    const Byte *metadata;
    Size metadata_size;
} NativeSections;

static NativeSections read_sections(const Byte *file, Size size, U32 node_word,
                                    int native)
{
    /* Both experiment profiles use little-endian SH16 descriptors. */
    enum { NODE=8, MEMBER=14, ENTRY_SIZE=6 };
    REQUIRE(size >= 36 && le32(file) == 0x014d475f &&
            le16(file + 4) == 1 && le16(file + 6) == 1 &&
            le16(file + NODE + 4) == 0,
            "expected a one-node SH16 UEEC file\n");
    U32 word = le32(file + NODE);
    unsigned count = word >> 27;
    REQUIRE((word & 0x07ffffffU) == (node_word & 0x07ffffffU),
            "no compatible UEEC node\n");
    REQUIRE(count == 1 || (native && count == 2), "unsupported UEEC sections\n");

    Size header_desc = MEMBER + count * ENTRY_SIZE;
    Size code_desc = header_desc + ENTRY_SIZE;
    Size metadata_desc = code_desc + ENTRY_SIZE;
    Size names = header_desc + (count + 1) * ENTRY_SIZE;
    REQUIRE(size >= names + 4 && le16(file + MEMBER) == 0 &&
            le32(file + MEMBER + 2) == 1 &&
            le16(file + header_desc) == 0 && le16(file + code_desc) == 1,
            "invalid UEEC section directory\n");
    if (count == 2) {
        REQUIRE(le16(file + MEMBER + ENTRY_SIZE) == 0 &&
                le32(file + MEMBER + ENTRY_SIZE + 2) == 2 &&
                le16(file + metadata_desc) == 13,
                "invalid UEEC metadata section\n");
    }
    Size header_span = le16(file + header_desc + 2);
    Size header_size = le16(file + header_desc + 4);
    Size code_span = le16(file + code_desc + 2);
    Size code_size = le16(file + code_desc + 4);
    REQUIRE(file[names] != 0 && le16(file + names + 1) == 0 &&
            header_size == names + 3 + file[names] &&
            header_size <= header_span && header_span <= size &&
            code_size != 0 && code_size <= MAX_NATIVE && code_size <= code_span &&
            code_span <= size - header_span,
            "invalid compact UEEC section spans\n");
    Size metadata_offset = header_span + code_span;
    NativeSections result = {file + header_span, code_size, 0, 0};
    if (count == 2) {
        Size metadata_span = le16(file + metadata_desc + 2);
        Size metadata_size = le16(file + metadata_desc + 4);
        REQUIRE(metadata_size != 0 && metadata_size <= metadata_span &&
                metadata_span == size - metadata_offset,
                "invalid UEEC metadata span\n");
        result.metadata = file + metadata_offset;
        result.metadata_size = metadata_size;
    } else {
        REQUIRE(metadata_offset == size, "invalid compact UEEC section spans\n");
    }
    if (!native) {
        REQUIRE(header_size == header_span && code_size == code_span &&
                code_size <= 512 && code_size % 2 == 0,
                "invalid compact UEEC section spans\n");
    }
    return result;
}

static Size parse_decimal(const char *p, Size limit)
{
    Size value = 0;
    REQUIRE(*p != '\0', "missing numeric argument\n");
    for (; *p; ++p) {
        REQUIRE(*p >= '0' && *p <= '9', "invalid numeric argument\n");
        Size digit = (Size)(*p - '0');
        REQUIRE(value <= (limit - digit) / 10, "numeric argument out of range\n");
        value = 10 * value + digit;
    }
    return value;
}

#endif
