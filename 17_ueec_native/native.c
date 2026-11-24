/* Load-time compiler for the CALL/MOV/RET/exit experiment. Linux only. */
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
    MAX_CODE=512, MAX_NATIVE=MAX_CODE * 16, MAX_CALLS=MAX_CODE / 4
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
    SWord address = linux_call(SYS_MAP, 0, length, protection, flags, fd, 0);
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

static void put32(Byte *p, U32 value)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (unsigned i = 0; i < 4; ++i) p[i] = (Byte)(value >> (8 * i));
#else
    for (unsigned i = 0; i < 4; ++i) p[3 - i] = (Byte)(value >> (8 * i));
#endif
}

typedef struct {
    U32 location;
    U32 vm_target;
} CallFixup;

typedef struct {
    U32 offsets[MAX_CODE / 2];
    CallFixup calls[MAX_CALLS];
    Size count;
} Relocations;

typedef struct {
    Byte *bytes;
    Size size;
} Code;

#if defined(__i386__) || defined(__x86_64__)
static void emit8(Code *code, Byte value)
{
    code->bytes[code->size++] = value;
}
#endif

static void emit32(Code *code, U32 value)
{
    put32(code->bytes + code->size, value);
    code->size += 4;
}

/* VM r1 lives in the Linux exit argument register, or in SPARC g3. */
static void emit_mov(Code *code, U32 value)
{
#if defined(__x86_64__)
    emit8(code, 0xbf); emit32(code, value);             /* mov edi, imm32 */
#elif defined(__i386__)
    emit8(code, 0xbb); emit32(code, value);             /* mov ebx, imm32 */
#elif defined(__arm__)
    emit32(code, 0xe3a00000 | value);                   /* mov r0, #imm8 */
#elif defined(__aarch64__)
    emit32(code, 0x52800000 | (value << 5));            /* movz w0, #imm */
#elif defined(__riscv)
    emit32(code, 0x00000513 | (value << 20));           /* addi a0, zero, imm */
#elif defined(__loongarch__)
    emit32(code, 0x02c00004 | (value << 10));           /* addi.d a0, zero, imm */
#elif defined(__powerpc__)
    emit32(code, 0x38600000 | value);                   /* li r3, imm */
#elif defined(__sparc__)
    emit32(code, 0x86102000 | value);                   /* or g0, imm, g3 */
#endif
}

static U32 emit_call(Code *code)
{
    /* Save each ISA's link register around nested calls. x86 CALL does so
       on its own. SPARC uses a new register window and a delay slot. */
#if defined(__arm__)
    emit32(code, 0xe92d4010); /* push {r4,lr} */
#elif defined(__aarch64__)
    emit32(code, 0xf81f0ffe); /* str x30, [sp,#-16]! */
#elif defined(__riscv)
    emit32(code, 0xff010113); /* addi sp,sp,-16 */
#if __riscv_xlen == 64
    emit32(code, 0x00113423); /* sd ra,8(sp) */
#else
    emit32(code, 0x00112223); /* sw ra,4(sp) */
#endif
#elif defined(__loongarch__)
    emit32(code, 0x02ffc063); /* addi.d sp,sp,-16 */
    emit32(code, 0x29c02061); /* st.d ra,sp,8 */
#elif defined(__powerpc__)
    emit32(code, 0x7c0802a6); /* mflr r0 */
#if defined(__powerpc64__)
    emit32(code, 0xf821ffe1); /* stdu r1,-32(r1) */
    emit32(code, 0xf8010010); /* std r0,16(r1) */
#else
    emit32(code, 0x9421ffe0); /* stwu r1,-32(r1) */
    emit32(code, 0x90010010); /* stw r0,16(r1) */
#endif
#elif defined(__sparc__)
    emit32(code, __SIZEOF_POINTER__ == 8 ? 0x9de3bf50 : 0x9de3bfa0); /* save */
#endif
#if defined(__x86_64__) || defined(__i386__)
    emit8(code, 0xe8);
#endif
    U32 location = (U32)code->size;
    emit32(code, 0);
#if defined(__arm__)
    emit32(code, 0xe8bd4010); /* pop {r4,lr} */
#elif defined(__aarch64__)
    emit32(code, 0xf84107fe); /* ldr x30,[sp],#16 */
#elif defined(__riscv)
#if __riscv_xlen == 64
    emit32(code, 0x00813083); /* ld ra,8(sp) */
#else
    emit32(code, 0x00412083); /* lw ra,4(sp) */
#endif
    emit32(code, 0x01010113); /* addi sp,sp,16 */
#elif defined(__loongarch__)
    emit32(code, 0x28c02061); /* ld.d ra,sp,8 */
    emit32(code, 0x02c04063); /* addi.d sp,sp,16 */
#elif defined(__powerpc__)
#if defined(__powerpc64__)
    emit32(code, 0xe8010010); /* ld r0,16(r1) */
#else
    emit32(code, 0x80010010); /* lwz r0,16(r1) */
#endif
    emit32(code, 0x7c0803a6); /* mtlr r0 */
    emit32(code, 0x38210020); /* addi r1,r1,32 */
#elif defined(__sparc__)
    emit32(code, 0x01000000); /* nop: call delay slot */
    emit32(code, 0x81e80000); /* restore */
#endif
    return location;
}

static void patch_call(Byte *code, U32 location, U32 target)
{
    /* This experiment emits at most MAX_NATIVE bytes, well inside each
       branch's displacement range. Both offsets are within that mapping. */
    int delta = (int)target - (int)location;
#if defined(__x86_64__) || defined(__i386__)
    U32 instruction = (U32)(delta - 4);
#elif defined(__arm__)
    U32 instruction = 0xeb000000 | ((U32)((delta - 8) / 4) & 0xffffff);
#elif defined(__aarch64__)
    U32 instruction = 0x94000000 | ((U32)(delta / 4) & 0x3ffffff);
#elif defined(__riscv)
    U32 imm = (U32)delta;
    U32 instruction = 0xef | ((imm & 0x100000) << 11) |
                       ((imm & 0x7fe) << 20) | ((imm & 0x800) << 9) |
                       (imm & 0xff000);
#elif defined(__loongarch__)
    U32 imm = (U32)(delta / 4) & 0x3ffffff;
    U32 instruction = 0x54000000 | ((imm & 0xffff) << 10) | (imm >> 16);
#elif defined(__powerpc__)
    U32 instruction = 0x48000001 | ((U32)delta & 0x03fffffc);
#elif defined(__sparc__)
    U32 instruction = 0x40000000 | ((U32)(delta / 4) & 0x3fffffff);
#endif
    put32(code + location, instruction);
}

static void emit_return(Code *code)
{
#if defined(__x86_64__) || defined(__i386__)
    emit8(code, 0xc3);
#elif defined(__arm__)
    emit32(code, 0xe12fff1e); /* bx lr */
#elif defined(__aarch64__)
    emit32(code, 0xd65f03c0); /* ret */
#elif defined(__riscv)
    emit32(code, 0x00008067); /* ret */
#elif defined(__loongarch__)
    emit32(code, 0x4c000020); /* jr ra */
#elif defined(__powerpc__)
    emit32(code, 0x4e800020); /* blr */
#elif defined(__sparc__)
    emit32(code, 0x81c3e008); /* retl */
    emit32(code, 0x01000000); /* delay slot */
#endif
}

static void emit_exit(Code *code)
{
#if defined(__x86_64__) || defined(__i386__)
    emit8(code, 0xb8); emit32(code, SYS_EXIT);
#if defined(__x86_64__)
    emit8(code, 0x0f); emit8(code, 0x05);               /* syscall */
#else
    emit8(code, 0xcd); emit8(code, 0x80);               /* int 0x80 */
#endif
    emit8(code, 0x0f); emit8(code, 0x0b);               /* ud2 */
#elif defined(__arm__)
    emit32(code, 0xe3a07000 | SYS_EXIT); emit32(code, 0xef000000);
    emit32(code, 0xe7f000f0);                          /* udf #0 */
#elif defined(__aarch64__)
    emit32(code, 0x52800008 | (SYS_EXIT << 5)); emit32(code, 0xd4000001);
    emit32(code, 0xd4200000);                          /* brk #0 */
#elif defined(__riscv)
    emit32(code, 0x00000893 | (SYS_EXIT << 20)); emit32(code, 0x00000073);
    emit32(code, 0);
#elif defined(__loongarch__)
    emit32(code, 0x02c0000b | (SYS_EXIT << 10)); emit32(code, 0x002b0000);
    emit32(code, 0x002a0000);                          /* break 0 */
#elif defined(__powerpc__)
    emit32(code, 0x38000000 | SYS_EXIT); emit32(code, 0x44000002);
    emit32(code, 0x7fe00008);                          /* trap */
#elif defined(__sparc__)
    emit32(code, 0x90100003);                          /* mov g3,o0 */
    emit32(code, 0x82102000 | SYS_EXIT);
    emit32(code, __SIZEOF_POINTER__ == 8 ? 0x91d0206d : 0x91d02010);
    emit32(code, 0);                                   /* unimp 0 */
#endif
}

static void sync_code(Byte *p, Size size)
{
#if defined(__arm__)
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

enum { EXIT=0, MOV=0x0802, CALL=0x0806, RETURN=0x0807 };

static Size instruction_size(const Byte *code, Size remaining)
{
    REQUIRE(remaining >= 4, "truncated bytecode\n");
    switch (le16(code)) {
    case EXIT:
        REQUIRE(le16(code + 2) == 0, "unsupported VM service\n");
        return 4;
    case RETURN:
        REQUIRE(le16(code + 2) == 0, "unsupported VM stack cleanup\n");
        return 4;
    case CALL:
        return 4;
    case MOV:
        REQUIRE(remaining >= 6 && le16(code + 2) == 1 && le16(code + 4) <= 255,
                "only MOV r1, status in 0..255 is supported\n");
        return 6;
    default:
        REQUIRE(0, "unsupported VM opcode or operand format\n");
        return 0;
    }
}

static U32 call_target(const Byte *code, U32 start, Size size)
{
    unsigned immediate = le16(code + start + 2);
    int units = (int)immediate - (immediate >= 32768 ? 65536 : 0);
    int target = (int)start + 4 + 2 * units;
    REQUIRE(target >= 0 && (Size)target < size, "CALL target outside VM CODE\n");
    return (U32)target;
}

static void validate_path(const Byte *code, Size size, const Relocations *relocs)
{
    /* This subset has unconditional control flow. Bound the executed path
       before transferring control and forbid RET without a caller. */
    U32 returns[32];
    Size depth = 0;
    U32 pc = 0;
    for (unsigned step = 0; step < 4096; ++step) {
        REQUIRE(pc < size && relocs->offsets[pc / 2], "invalid VM instruction boundary\n");
        switch (le16(code + pc)) {
        case EXIT:
            return;
        case CALL:
            REQUIRE(depth < sizeof(returns) / sizeof(*returns), "VM call depth exceeded\n");
            returns[depth++] = pc + 4;
            pc = call_target(code, pc, size);
            break;
        case RETURN:
            REQUIRE(depth != 0, "VM RET without CALL\n");
            pc = returns[--depth];
            break;
        default:
            pc += 6;
            break;
        }
    }
    REQUIRE(0, "VM execution budget exceeded\n");
}

static void compile(const Byte *code, Size size, Code *out, Relocations *relocs)
{
    emit_mov(out, 0);
    for (U32 pc = 0; pc < size;) {
        Size length = instruction_size(code + pc, size - pc);
        relocs->offsets[pc / 2] = (U32)out->size;
        switch (le16(code + pc)) {
        case EXIT:
            emit_exit(out);
            break;
        case MOV:
            emit_mov(out, le16(code + pc + 4));
            break;
        case CALL: {
            CallFixup *fix = &relocs->calls[relocs->count++];
            fix->location = emit_call(out);
            fix->vm_target = call_target(code, pc, size);
            break;
        }
        case RETURN:
            emit_return(out);
            break;
        }
        pc += (U32)length;
    }
    validate_path(code, size, relocs);
    for (Size i = 0; i < relocs->count; ++i) {
        CallFixup fix = relocs->calls[i];
        U32 target = relocs->offsets[fix.vm_target / 2];
        REQUIRE(target != 0, "CALL targets a VM operand\n");
        patch_call(out->bytes, fix.location, target);
    }
}

static const Byte *locate_code(const Byte *file, Size size, Size *length)
{
    /* The experiment writer emits one compact VM node and one SH16 CODE.
       These offsets follow the existing generate.container() writer. */
    enum { NODE=8, MEMBER=14, HEADER=20, CODE=26, NAMES=32 };
    REQUIRE(size >= NAMES + 4 && le32(file) == 0x014d475f &&
            le16(file + 4) == 1 && le16(file + 6) == 1 &&
            le32(file + NODE) == 0x0800012c && le16(file + NODE + 4) == 0 &&
            le16(file + MEMBER) == 0 && le32(file + MEMBER + 2) == 1 &&
            le16(file + HEADER) == 0 && le16(file + CODE) == 1,
            "expected a one-node SH16 UEEC VM fixture\n");
    Size header = le16(file + HEADER + 2);
    *length = le16(file + CODE + 4);
    REQUIRE(header == (Size)NAMES + 3 + file[NAMES] && file[NAMES] != 0 &&
            le16(file + NAMES + 1) == 0 && header == le16(file + HEADER + 4) &&
            header <= size && *length == size - header &&
            *length == le16(file + CODE + 2) &&
            *length > 0 && *length <= MAX_CODE && *length % 2 == 0,
            "invalid compact UEEC section spans\n");
    return file + header;
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

void run(Size argc, const char **argv) __attribute__((noreturn));
void run(Size argc, const char **argv)
{
    REQUIRE(argc == 3, "usage: native FILE_SIZE READ_ONLY_FD\n");
    Size file_size = parse_decimal(argv[1], 1024);
    Size fd = parse_decimal(argv[2], 1048575);
    REQUIRE(file_size > 0, "invalid file size\n");
    Byte *input = map(file_size, PROT_R, MAP_PRIVATE, fd);
    Size size;
    const Byte *bytecode = locate_code(input, file_size, &size);

    Code generated = {map(MAX_NATIVE, PROT_R | PROT_W, MAP_PRIVATE | MAP_ANONYMOUS, (Word)-1), 0};
    Relocations *relocs = (void *)map(sizeof(*relocs), PROT_R | PROT_W,
                                    MAP_PRIVATE | MAP_ANONYMOUS, (Word)-1);
    compile(bytecode, size, &generated, relocs);
    sync_code(generated.bytes, generated.size);
    REQUIRE(sys3(SYS_PROTECT, (Word)generated.bytes, generated.size, PROT_R | PROT_X) == 0,
            "mprotect RX failed\n");
    release(input, file_size);
    release((void *)relocs, sizeof(*relocs));
    SAY("native CODE is RX; VM CODE and relocations unmapped\n");
    enter_code(generated.bytes);
}
