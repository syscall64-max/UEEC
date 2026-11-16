#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "vm_runtime.h"
#include <stdarg.h>
#include <limits.h>

#define CMD_SYSCALL          0
#define CMD_MOV_INDEX_INDEX  1
#define CMD_MOV_INDEX_IMM    2
#define CMD_PUSH_INDEX       3
#define CMD_PUSH_IMM         4
#define CMD_POP_INDEX        5
#define CMD_CALL             6
#define CMD_RET              7

/* Operand format 0 is reserved; SYSCALL retains its fixed format=0. */
#define FORMAT_16  1
#define FORMAT_32  2
#define FORMAT_64  3

#define REG_INDEX    0
#define IDATA_INDEX  1
#define STACK_INDEX  2
#define HEAP_INDEX   3

#define VM_REG_COUNT  16
#define VM_STACK_SIZE 4096
#define VM_FRAME_LIMIT 512

typedef struct {
    uint8_t type;
    uint64_t index;
} vm_index;

typedef struct {
    uint64_t reg[VM_REG_COUNT];
} gp_regs;

/*
 * CALL metadata is NOT stored in the VM data stack.
 *
 * This prevents frame-linkage corruption when the VM stack contains
 * 8/16/32/64-bit values with different physical block counts.
 */
typedef struct vm_call_frame {
    uintptr_t return_ip;
    uintptr_t old_bp;

    /*
     * SP before this call's arguments were pushed.
     * RET cleanup returns SP to this exact position.
     */
    uintptr_t caller_sp;

    /*
     * First parameter block for this callee.
     */
    uintptr_t arg_base;

    /*
     * Number of physical 16-bit blocks passed to this call.
     * Used for validation/debugging.
     */
    uintptr_t arg_blocks;

    struct vm_call_frame *prev;
} vm_call_frame;

typedef struct {
    gp_regs regs;

    /*
     * All VM addressing units below are 16-bit blocks where applicable.
     */
    uintptr_t ip;       /* CODE block index */
    uintptr_t stack;    /* host/native base address of thread stack */
    uintptr_t sp;       /* physical 16-bit block index */
    uintptr_t bp;       /* first parameter block of current function */

    /*
     * PUSH instructions accumulate argument blocks for the next CALL.
     * After CALL it resets to zero.
     *
     * ABI rule:
     *   stack arguments for a CALL are the contiguous PUSH blocks
     *   accumulated since the previous call boundary.
     */
    uintptr_t pending_arg_blocks;

    vm_call_frame *frame;
    size_t frame_depth;
    size_t frame_limit;

    size_t stack_size;

    int exit_status;
    int finished;
    int trace;
    uint64_t steps, max_steps;
} vm_thread;

typedef struct {
    const uint8_t *code;
    size_t code_size;

    uint8_t *idata;
    size_t idata_size;
} vm_process;


/* Tracing is per execution context; no mutable global VM state. */
static void vm_trace(const vm_thread *t, const char *format, ...)
{
    if (t->trace) {
        va_list ap;
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

/* VM cells have canonical little-endian storage, independent of host order. */
static uint64_t cell_read(const void *memory, size_t bytes)
{
    const uint8_t *p = memory;
    uint64_t value = 0;
    for (size_t i=0; i<bytes; ++i) value |= (uint64_t)p[i] << (8*i);
    return value;
}
static void cell_write(void *memory, size_t bytes, uint64_t value)
{
    uint8_t *p = memory;
    for (size_t i=0; i<bytes; ++i) p[i] = (uint8_t)(value >> (8*i));
}

/* ============================================================
 * Basic readers
 * ============================================================ */

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0]
         | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const uint8_t *p)
{
    return (uint64_t)rd32(p)
         | ((uint64_t)rd32(p + 4) << 32);
}


/* ============================================================
 * FORMAT rules
 * ============================================================ */

/*
 * Encoded operand size inside CODE.
 *
 * FORMAT16/32/64 operands use 2/4/8 encoded bytes. Format 0 is reserved.
 */
static size_t encoded_bytes(uint16_t format)
{
    if (format < FORMAT_16 || format > FORMAT_64)
        return 0;

    return (size_t)1u << format;
}

/*
 * Physical VM memory size.
 *
 * VM memory granularity is always 16 bits:
 *
 * FORMAT16 => 2 physical bytes
 * FORMAT32 => 4 physical bytes
 * FORMAT64 => 8 physical bytes
 */
static size_t memory_bytes(uint16_t format)
{
    switch (format) {
    case FORMAT_16:
        return 2;

    case FORMAT_32:
        return 4;

    case FORMAT_64:
        return 8;

    default:
        return 0;
    }
}

static size_t memory_blocks(uint16_t format)
{
    size_t n = memory_bytes(format);
    return n ? (n >> 1) : 0;
}

static size_t ins1_bytes(uint16_t format)
{
    size_t n = 2 + encoded_bytes(format);
    return (n + 1u) & ~1u;
}

static size_t ins2_bytes(uint16_t format)
{
    size_t n = 2 + 2 * encoded_bytes(format);
    return (n + 1u) & ~1u;
}

static uint64_t read_encoded_u(const uint8_t *p, uint16_t format)
{
    switch (format) {
    case FORMAT_16:
        return rd16(p);

    case FORMAT_32:
        return rd32(p);

    case FORMAT_64:
        return rd64(p);

    default:
        return 0;
    }
}

static int64_t read_encoded_s(const uint8_t *p, uint16_t format)
{
    switch (format) {
    case FORMAT_16:
        return (int16_t)rd16(p);

    case FORMAT_32:
        return (int32_t)rd32(p);

    case FORMAT_64:
        return (int64_t)rd64(p);

    default:
        return 0;
    }
}


/* ============================================================
 * Index decoding
 * ============================================================ */

static vm_index decode_index(const uint8_t *p, uint16_t format)
{
    size_t n = encoded_bytes(format);
    unsigned bits = (unsigned)(n * 8u);
    uint64_t raw = read_encoded_u(p, format);

    vm_index x = {0};

    x.type = (uint8_t)((raw >> (bits - 2u)) & 3u);

    if (bits == 64)
        x.index = raw & UINT64_C(0x3fffffffffffffff);
    else
        x.index =
            raw & ((UINT64_C(1) << (bits - 2u)) - 1u);

    return x;
}


/* ============================================================
 * VM memory access
 * ============================================================ */

static void *resolve_memory(
    vm_process *p,
    vm_thread *t,
    vm_index x,
    size_t physical_bytes)
{
    switch (x.type) {
    case IDATA_INDEX: {
        if (x.index > SIZE_MAX/2) return NULL;
        size_t off = (size_t)x.index << 1;

        if (off > p->idata_size ||
            physical_bytes > p->idata_size - off)
            return NULL;

        return p->idata + off;
    }

    case STACK_INDEX: {
        if (x.index > SIZE_MAX/2 || t->bp > SIZE_MAX/2 - (size_t)x.index) return NULL;
        size_t block =
            (size_t)t->bp + (size_t)x.index;

        size_t off = block << 1;

        if (off > t->stack_size ||
            physical_bytes > t->stack_size - off)
            return NULL;

        return (uint8_t *)t->stack + off;
    }

    case HEAP_INDEX:
    default:
        return NULL;
    }
}


static int read_index_value(
    vm_process *p,
    vm_thread *t,
    vm_index x,
    uint16_t format,
    uint64_t *value)
{
    size_t logical = encoded_bytes(format);
    size_t physical = memory_bytes(format);

    if (!logical || !physical)
        return -1;

    if (x.type == REG_INDEX) {
        if (x.index >= VM_REG_COUNT)
            return -1;

        uint64_t v = (uint64_t)t->regs.reg[x.index];

        switch (format) {
        case FORMAT_16:
            v &= UINT64_C(0xffff);
            break;
        case FORMAT_32:
            v &= UINT64_C(0xffffffff);
            break;
        case FORMAT_64:
            break;
        default:
            return -1;
        }

        *value = v;
        return 0;
    }

    void *ptr =
        resolve_memory(p, t, x, physical);

    if (!ptr)
        return -1;

    uint64_t v = 0;

    /*
     * Read whole physical VM cell(s).
     */
    v = cell_read(ptr, physical);


    *value = v;
    return 0;
}


static int write_index_value(
    vm_process *p,
    vm_thread *t,
    vm_index x,
    uint16_t format,
    uint64_t value)
{
    size_t physical = memory_bytes(format);

    if (!physical)
        return -1;

    switch (format) {
    case FORMAT_16:
        value &= UINT64_C(0xffff);
        break;

    case FORMAT_32:
        value &= UINT64_C(0xffffffff);
        break;

    case FORMAT_64:
        break;

    default:
        return -1;
    }

    if (x.type == REG_INDEX) {
        if (x.index >= VM_REG_COUNT)
            return -1;

        t->regs.reg[x.index] = value;
        return 0;
    }

    void *ptr =
        resolve_memory(p, t, x, physical);

    if (!ptr)
        return -1;


    cell_write(ptr, physical, value);

    return 0;
}


/* ============================================================
 * Data stack
 * ============================================================ */

static int stack_push_value(
    vm_thread *t,
    uint64_t value,
    uint16_t format)
{
    size_t bytes = memory_bytes(format);
    size_t blocks = memory_blocks(format);

    if (!bytes || !blocks)
        return -1;

    switch (format) {
    case FORMAT_16:
        value &= UINT64_C(0xffff);
        break;

    case FORMAT_32:
        value &= UINT64_C(0xffffffff);
        break;

    case FORMAT_64:
        break;

    default:
        return -1;
    }

    size_t off = (size_t)t->sp << 1;

    if (off > t->stack_size ||
        bytes > t->stack_size - off)
        return -1;


    cell_write((uint8_t *)t->stack + off, bytes, value);

    t->sp += blocks;
    t->pending_arg_blocks += blocks;

    return 0;
}

static int stack_pop_value(
    vm_thread *t,
    uint16_t format,
    uint64_t *value)
{
    size_t bytes = memory_bytes(format);
    size_t blocks = memory_blocks(format);

    if (!bytes || !blocks || t->sp < blocks)
        return -1;

    t->sp -= blocks;

    size_t off = (size_t)t->sp << 1;

    uint64_t v = 0;

    v = cell_read((uint8_t *)t->stack + off, bytes);


    *value = v;

    /*
     * POP cancels pending argument blocks if they have not yet
     * been consumed by CALL.
     */
    if (t->pending_arg_blocks >= blocks)
        t->pending_arg_blocks -= blocks;
    else
        t->pending_arg_blocks = 0;

    return 0;
}


/* ============================================================
 * CALL frame chain
 * ============================================================ */

static int vm_call(
    vm_thread *t,
    uintptr_t next_ip,
    uintptr_t target_ip)
{
    if (t->pending_arg_blocks > t->sp)
        return -1;

    /*
     * Hard per-thread CALL-frame limit.
     *
     * Frame metadata is intentionally isolated from the VM data stack,
     * but recursion must still be bounded so a program cannot grow the
     * host heap without limit.
     */
    if (t->frame_depth >= t->frame_limit) {
        fprintf(
            stderr,
            "CALL frame overflow: depth=%zu limit=%zu\n",
            t->frame_depth,
            t->frame_limit);

        return -1;
    }

    vm_call_frame *f =
        calloc(1, sizeof(*f));

    if (!f)
        return -1;

    f->return_ip = next_ip;
    f->old_bp = t->bp;

    f->arg_blocks =
        t->pending_arg_blocks;

    f->arg_base =
        t->sp - f->arg_blocks;

    f->caller_sp =
        f->arg_base;

    f->prev = t->frame;

    t->frame = f;
    t->frame_depth++;

    t->bp = f->arg_base;
    t->pending_arg_blocks = 0;
    t->ip = target_ip;

    return 0;
}

static int vm_ret(
    vm_thread *t,
    uintptr_t cleanup_blocks)
{
    vm_call_frame *f = t->frame;

    if (!f)
        return -1;

    /*
     * Final ABI:
     *
     * RET imm is the number of 16-bit argument blocks to remove.
     * It must not exceed the number of blocks that CALL attached
     * to this frame.
     */
    if (cleanup_blocks > f->arg_blocks)
        return -1;

    /*
     * Any callee temporaries that remain above its argument area
     * are discarded automatically because return SP is rebuilt
     * from frame metadata, not from current SP.
     *
     * This is exactly why CALL metadata is separate from data stack.
     */
    uintptr_t new_sp =
        f->arg_base + f->arg_blocks - cleanup_blocks;

    t->sp = new_sp;
    t->bp = f->old_bp;
    t->ip = f->return_ip;

    t->frame = f->prev;

    free(f);

    if (t->frame_depth)
        t->frame_depth--;

    /*
     * Returned caller starts with no pending arguments.
     */
    t->pending_arg_blocks = 0;

    return 0;
}

static void free_frames(vm_thread *t)
{
    while (t->frame) {
        vm_call_frame *f = t->frame;
        t->frame = f->prev;
        free(f);

        if (t->frame_depth)
            t->frame_depth--;
    }
}


static int relative_target(size_t next, int64_t rel, size_t blocks, size_t *target)
{
    if (rel < 0) {
        uint64_t delta = (uint64_t)(-(rel+1)) + 1;
        if (delta > next) return -1;
        *target = next - (size_t)delta;
    } else {
        if ((uint64_t)rel > SIZE_MAX-next) return -1;
        *target = next + (size_t)rel;
    }
    return *target < blocks ? 0 : -1;
}

/* A CALL may only target an instruction boundary, not an operand/padding. */
static int validate_code(const vm_process *p)
{
    size_t blocks=p->code_size/2;
    if (!blocks || p->code_size%2) {
        fprintf(stderr,"invalid CODE length\n"); return -1;
    }
    uint8_t *boundary=calloc(blocks,1);
    if (!boundary) return -1;
    int rc=-1;
    for (size_t off=0; off<p->code_size;) {
        uint16_t raw=rd16(p->code+off), cmd=raw&0x7ff, fmt=raw>>11;
        if (cmd>CMD_RET || (cmd==CMD_SYSCALL ? fmt!=0 : (fmt<FORMAT_16 || fmt>FORMAT_64))) goto done;
        size_t bytes=cmd==CMD_SYSCALL ? 4 :
            ((cmd==CMD_MOV_INDEX_IMM || cmd==CMD_MOV_INDEX_INDEX) ? ins2_bytes(fmt) : ins1_bytes(fmt));
        if (bytes>p->code_size-off) goto done;
        boundary[off/2]=1; off+=bytes;
    }
    for (size_t off=0; off<p->code_size;) {
        uint16_t raw=rd16(p->code+off), cmd=raw&0x7ff, fmt=raw>>11;
        size_t bytes=cmd==CMD_SYSCALL ? 4 :
            ((cmd==CMD_MOV_INDEX_IMM || cmd==CMD_MOV_INDEX_INDEX) ? ins2_bytes(fmt) : ins1_bytes(fmt));
        if (cmd==CMD_CALL) {
            size_t target;
            if (relative_target((off+bytes)/2,read_encoded_s(p->code+off+2,fmt),blocks,&target) || !boundary[target]) goto done;
        }
        off+=bytes;
    }
    rc=0;
done:
    free(boundary);
    if (rc) fprintf(stderr,"invalid instruction or CALL target\n");
    return rc;
}

/* ============================================================
 * SYSCALL
 * ============================================================ */

/* VM ABI: 0=exit(r1); 1=write_stdout(r1=IDATA cell index,r2=byte count).
 * One output byte is read from the low byte of each 16-bit IDATA cell.
 * These are VM services, never native Linux syscall numbers/pointers.
 */
static int vm_syscall(vm_process *p, vm_thread *t, uint16_t nr)
{
    if (nr==0) {
        if (t->regs.reg[1]>255) { fprintf(stderr,"invalid exit status\n"); return -1; }
        t->exit_status=(int)t->regs.reg[1]; t->finished=1;
        vm_trace(t,"sys_exit status=%d\n",t->exit_status);
        return 0;
    }
    if (nr==1) {
        uint64_t start=t->regs.reg[1], count=t->regs.reg[2];
        if (start>p->idata_size/2 || count>p->idata_size/2-start) {
            fprintf(stderr,"write_stdout outside IDATA\n"); return -1;
        }
        for (uint64_t i=0;i<count;++i)
            if (putchar(p->idata[2*(size_t)(start+i)])==EOF) return -1;
        return 0;
    }
    fprintf(stderr,"unknown syscall %u\n",nr); return -1;
}

/* ============================================================
 * Interpreter
 * ============================================================ */

static int vm_run(vm_process *p, vm_thread *t)
{
    size_t code_blocks =
        p->code_size >> 1;

    while (!t->finished &&
           t->ip < code_blocks) {

        if (t->steps >= t->max_steps) {
            fprintf(stderr,"instruction limit reached\n"); return 1;
        }
        ++t->steps;
        size_t off =
            (size_t)t->ip << 1;

        if (off + 2 > p->code_size)
            return 1;

        uint16_t raw =
            rd16(p->code + off);

        uint16_t cmd =
            raw & 0x07ff;

        uint16_t fmt =
            raw >> 11;

        vm_trace(t,
            "ip=%" PRIuPTR
            " command=%u"
            " format=%u"
            " sp=%" PRIuPTR
            " bp=%" PRIuPTR
            " pending=%" PRIuPTR "\n",
            t->ip,
            cmd,
            fmt,
            t->sp,
            t->bp,
            t->pending_arg_blocks);

        if (cmd != CMD_SYSCALL &&
            (fmt < FORMAT_16 || fmt > FORMAT_64)) {

            fprintf(
                stderr,
                "reserved format %u\n",
                fmt);

            return 1;
        }

        switch (cmd) {

        case CMD_SYSCALL: {
            if (fmt != 0 ||
                off + 4 > p->code_size)
                return 1;

            uint16_t nr =
                rd16(p->code + off + 2);

            t->ip += 2;

            vm_trace(t,
                "syscall %u\n",
                nr);

            if (vm_syscall(p, t, nr))
                return 1;

            break;
        }

        case CMD_MOV_INDEX_IMM: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins2_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            vm_index dst =
                decode_index(
                    p->code + off + 2,
                    fmt);

            uint64_t imm =
                read_encoded_u(
                    p->code + off + 2 + eb,
                    fmt);

            if (write_index_value(
                    p,
                    t,
                    dst,
                    fmt,
                    imm)) {

                fprintf(
                    stderr,
                    "MOVi invalid target\n");

                return 1;
            }

            vm_trace(t,
                "MOVi type=%u"
                " index=%" PRIu64
                " value=%" PRIu64 "\n",
                dst.type,
                dst.index,
                imm);

            t->ip +=
                bytes >> 1;

            break;
        }

        case CMD_MOV_INDEX_INDEX: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins2_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            vm_index dst =
                decode_index(
                    p->code + off + 2,
                    fmt);

            vm_index src =
                decode_index(
                    p->code + off + 2 + eb,
                    fmt);

            uint64_t value = 0;

            if (read_index_value(
                    p,
                    t,
                    src,
                    fmt,
                    &value) ||
                write_index_value(
                    p,
                    t,
                    dst,
                    fmt,
                    value)) {

                fprintf(
                    stderr,
                    "MOV invalid index\n");

                return 1;
            }

            vm_trace(t,
                "MOV type=%u"
                " index=%" PRIu64
                " <- type=%u"
                " index=%" PRIu64
                " value=%" PRIu64 "\n",
                dst.type,
                dst.index,
                src.type,
                src.index,
                value);

            t->ip +=
                bytes >> 1;

            break;
        }

        case CMD_PUSH_IMM: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins1_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            uint64_t value =
                read_encoded_u(
                    p->code + off + 2,
                    fmt);

            if (stack_push_value(
                    t,
                    value,
                    fmt)) {

                fprintf(
                    stderr,
                    "PUSH immediate failed\n");

                return 1;
            }

            vm_trace(t,
                "PUSHi value=%" PRIu64
                " physical_blocks=%zu\n",
                value,
                memory_blocks(fmt));

            t->ip +=
                bytes >> 1;

            break;
        }

        case CMD_PUSH_INDEX: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins1_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            vm_index src =
                decode_index(
                    p->code + off + 2,
                    fmt);

            uint64_t value = 0;

            if (read_index_value(
                    p,
                    t,
                    src,
                    fmt,
                    &value) ||
                stack_push_value(
                    t,
                    value,
                    fmt)) {

                fprintf(
                    stderr,
                    "PUSH index failed\n");

                return 1;
            }

            t->ip +=
                bytes >> 1;

            break;
        }

        case CMD_POP_INDEX: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins1_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            vm_index dst =
                decode_index(
                    p->code + off + 2,
                    fmt);

            uint64_t value = 0;

            if (stack_pop_value(
                    t,
                    fmt,
                    &value) ||
                write_index_value(
                    p,
                    t,
                    dst,
                    fmt,
                    value)) {

                fprintf(
                    stderr,
                    "POP failed\n");

                return 1;
            }

            vm_trace(t,
                "POP value=%" PRIu64
                " physical_blocks=%zu\n",
                value,
                memory_blocks(fmt));

            t->ip +=
                bytes >> 1;

            break;
        }

        case CMD_CALL: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins1_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            int64_t relative =
                read_encoded_s(
                    p->code + off + 2,
                    fmt);

            uintptr_t next_ip =
                t->ip + (bytes >> 1);

            size_t target;
            if (relative_target(next_ip, relative, code_blocks, &target)) {

                fprintf(
                    stderr,
                    "CALL target out of bounds\n");

                return 1;
            }

            uintptr_t args =
                t->pending_arg_blocks;

            uintptr_t arg_base =
                t->sp - args;

            if (vm_call(
                    t,
                    next_ip,
                    (uintptr_t)target)) {

                fprintf(
                    stderr,
                    "CALL frame allocation failed\n");

                return 1;
            }

            vm_trace(t,
                "CALL relative=%" PRId64
                " target=%" PRIuPTR
                " arg_base=%" PRIuPTR
                " arg_blocks=%" PRIuPTR "\n",
                relative,
                t->ip,
                arg_base,
                args);

            break;
        }

        case CMD_RET: {
            size_t eb =
                encoded_bytes(fmt);

            size_t bytes =
                ins1_bytes(fmt);

            if (!eb ||
                off + bytes > p->code_size)
                return 1;

            uint64_t cleanup =
                read_encoded_u(
                    p->code + off + 2,
                    fmt);

            uintptr_t ret_ip =
                t->frame
                    ? t->frame->return_ip
                    : 0;

            if (cleanup > SIZE_MAX || vm_ret(
                    t,
                    (uintptr_t)cleanup)) {

                fprintf(
                    stderr,
                    "RET failed\n");

                return 1;
            }

            vm_trace(t,
                "RET cleanup=%" PRIu64
                " return=%" PRIuPTR
                " sp=%" PRIuPTR
                " bp=%" PRIuPTR "\n",
                cleanup,
                ret_ip,
                t->sp,
                t->bp);

            break;
        }

        default:
            fprintf(
                stderr,
                "unknown command %u\n",
                cmd);

            return 1;
        }
    }

    if (!t->finished) { fprintf(stderr,"CODE ended without sys_exit\n"); return 1; }
    return t->exit_status;
}


int ueec_vm_execute(const uint8_t *code, size_t code_size,
                    uint8_t *idata, size_t idata_size,
                    uint64_t max_steps, int trace)
{
    vm_process p={.code=code,.code_size=code_size,.idata=idata,.idata_size=idata_size};
    if (!max_steps || (idata_size%2) || validate_code(&p)) return 1;
    vm_thread t={0};
    t.stack_size=VM_STACK_SIZE; t.stack=(uintptr_t)calloc(1,t.stack_size);
    if (!t.stack) return 1;
    t.frame_limit=VM_FRAME_LIMIT; t.max_steps=max_steps; t.trace=trace;
    int rc=vm_run(&p,&t);
    printf("RESULT r4=%" PRIu64 " r5=%" PRIu64 " r6=%" PRIu64
           " r7=%" PRIu64 " r8=%" PRIu64 " r9=%" PRIu64
           " r10=%" PRIu64 " sp=%" PRIuPTR " bp=%" PRIuPTR " exit=%d steps=%" PRIu64 "\n",
           t.regs.reg[4],t.regs.reg[5],t.regs.reg[6],t.regs.reg[7],
           t.regs.reg[8],t.regs.reg[9],t.regs.reg[10],t.sp,t.bp,t.exit_status,t.steps);
    free_frames(&t); free((void *)t.stack);
    printf("VM context released: frames=%zu\n",t.frame_depth);
    return rc;
}
