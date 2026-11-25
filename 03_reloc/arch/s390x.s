/* Linux s390x position-independent raw code for UEEC stage 03.
 * The local literal 'data_address' is patched with an ABS64 relocation.
 * build: s390x-linux-gnu-as -o s390x.o s390x.s
 *        s390x-linux-gnu-objcopy -O binary -j .text s390x.o s390x.bin
 * For stable layout, use only the instructions/assembler alignment below.
 */
.text
.globl _start
_start:
    lghi %r2,1                /* fd = STDOUT_FILENO */
    larl %r3,data_address      /* address of the pointer literal */
    lg %r3,0(%r3)             /* load relocated pointer */
    lghi %r4,13               /* number of bytes */
    svc 4                     /* Linux sys_write */
    lghi %r2,0                /* exit status */
    svc 1                     /* Linux sys_exit */
    .balign 8
data_address:
    .quad 0                   /* offset 32: UEEC ABS64 relocation */
