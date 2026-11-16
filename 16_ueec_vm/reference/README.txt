UEF_VM STACK + FUNCTION CALLS FINAL v7
=======================================

This package consolidates the current final rules for:

    - 16-bit physical memory granularity
    - FORMAT8 memory representation
    - PUSH index / PUSH immediate
    - POP index
    - STACK_INDEX
    - BP / SP
    - relative CALL8/16/32/64
    - RET8/16/32/64 immediate cleanup
    - nested CALL
    - independent call-frame metadata
    - per-thread stack and frame chain


1. Physical memory granularity
------------------------------

UEF_VM memory has a minimum physical unit of 16 bits.

Logical value format     Physical memory
--------------------     ---------------
FORMAT8                  16 bit = 1 block
FORMAT16                 16 bit = 1 block
FORMAT32                 32 bit = 2 blocks
FORMAT64                 64 bit = 4 blocks

Therefore:

    PUSH8  -> writes 2 bytes
    PUSH16 -> writes 2 bytes
    PUSH32 -> writes 4 bytes
    PUSH64 -> writes 8 bytes

For FORMAT8:

    logical value = 0x7A
    physical cell = 0x007A

Little-endian bytes:

    7A 00

The high 8 bits are zero.


2. CODE operand width is separate
---------------------------------

Instruction operands inside CODE keep their encoded width:

    FORMAT8  operand = 1 byte
    FORMAT16 operand = 2 bytes
    FORMAT32 operand = 4 bytes
    FORMAT64 operand = 8 bytes

CODE itself remains aligned to 16-bit blocks.


3. Stack registers
------------------

Thread owns:

    stack  = stack base address
    sp     = 16-bit physical block index
    bp     = first parameter block of current function

Initial state:

    sp = 0
    bp = 0


4. STACK_INDEX
--------------

STACK_INDEX is an unsigned 16-bit-block index relative to BP.

Address:

    stack + ((bp + index) << 1)

Examples:

    STACK_INDEX[0] -> BP + 0 bytes
    STACK_INDEX[1] -> BP + 2 bytes
    STACK_INDEX[2] -> BP + 4 bytes


5. PUSH
-------

Commands:

    PUSH index
    PUSH imm

Source spelling:

    PUSH8
    PUSH8i
    PUSH16
    PUSH16i
    PUSH32
    PUSH32i
    PUSH64
    PUSH64i

Each PUSH increments SP by physical blocks:

    FORMAT8  -> +1
    FORMAT16 -> +1
    FORMAT32 -> +2
    FORMAT64 -> +4


6. POP
------

Command:

    POP index

Source spelling:

    POP8
    POP16
    POP32
    POP64

POP decrements SP by the corresponding physical block count.


7. CALL
-------

Command:

    CALL relative_index

Source spelling:

    CALL8
    CALL16
    CALL32
    CALL64

CALL operand is SIGNED.

Reference point:

    next_instruction_index

Calculation:

    target_index =
        next_instruction_index + relative_index

CODE index unit:

    1 index = 1 x 16-bit block

No direction bit is required.
Signed operand already carries direction.


8. Function stack arguments
---------------------------

PUSH blocks accumulated before CALL become that CALL's stack arguments.

CALL computes:

    arg_blocks = pending_arg_blocks

    arg_base =
        sp - arg_blocks

Then:

    bp = arg_base

So callee sees:

    STACK_INDEX[0] = first pushed argument block
    STACK_INDEX[1] = next block
    ...

Mixed widths are block-based.

Example:

    PUSH8i  0x55       ; 1 block
    PUSH16i 333        ; 1 block
    PUSH32i 0x12345678 ; 2 blocks

Callee layout:

    STACK_INDEX[0] = 0x0055
    STACK_INDEX[1] = 333
    STACK_INDEX[2..3] = 0x12345678


9. CALL frame metadata
----------------------

IMPORTANT FINAL RULE:

Return IP and previous BP are NOT stored inside the VM data stack.

They are stored in a per-thread private frame chain:

    vm_call_frame {
        return_ip
        old_bp
        caller_sp
        arg_base
        arg_blocks
        prev
    }

Reason:

The VM data stack contains variable-width values:
    1, 2, or 4 physical blocks.

Keeping hidden frame metadata in the same data stack makes nested calls
and arbitrary stack contents much harder to keep structurally safe.

Separate frame metadata means:

    - nested calls are safe
    - FORMAT8/16/32/64 pushes cannot corrupt return linkage
    - recursion is naturally representable
    - RET can rebuild SP from frame metadata
    - thread owns its complete call chain


10. RET imm
-----------

Command:

    RET imm

Source spelling:

    RET8
    RET16
    RET32
    RET64

Immediate value is the number of PHYSICAL 16-bit stack blocks to clean.

Example:

    RET16 4

means:

    cleanup 4 blocks
    cleanup 8 physical bytes

RET restores:

    previous BP
    return IP

and updates SP from saved frame information.


11. Registers
-------------

Registers remain thread-local:

    uintptr_t reg[16]

They are not stored in IDATA.


12. Test program
----------------

This package tests:

A) FORMAT8 physical stack cell:

    PUSH8i 0x7A
    POP8 r9

Expected:

    r9 = 122


B) function1(111, 222)

Expected:

    r4 = 111
    r5 = 222


C) nested function2:

    PUSH8i  0x55
    PUSH16i 333
    PUSH32i 0x12345678

    CALL16 function2

function2 reads:

    r6  = STACK_INDEX[0] as FORMAT8
    r7  = STACK_INDEX[1] as FORMAT16
    r10 = STACK_INDEX[2] as FORMAT32

Expected:

    r6  = 85
    r7  = 333
    r10 = 305419896


D) after nested RET, function1 BP must be restored:

    r8 = STACK_INDEX[0]

Expected:

    r8 = 111


E) function1 RET16 2 cleans its two incoming blocks.


13. Expected final RESULT
-------------------------

Approximately:

    r4=111
    r5=222
    r6=85
    r7=333
    r8=111
    r9=122
    r10=305419896

The final sys_exit call does not RET, so its frame can remain alive until
thread teardown; free_frames() removes any remaining frame descriptors.


14. Build
---------

    make run


15. Command map
---------------

    0 = SYSCALL
    1 = MOV index,index
    2 = MOV index,imm
    3 = PUSH index
    4 = PUSH imm
    5 = POP index
    6 = CALL signed-relative
    7 = RET imm


16. Bounded CALL-frame depth
----------------------------

CALL metadata remains outside the VM data stack, but it is NOT unbounded.

Each thread has:

    frame_depth
    frame_limit

Default proof-of-concept limit:

    VM_FRAME_LIMIT = 512

Before allocating a new vm_call_frame, CALL checks:

    if (frame_depth >= frame_limit)
        "CALL frame overflow"

Successful CALL:

    frame_depth++

RET:

    frame_depth--

Thread teardown also frees any remaining frames and decrements the count.

This preserves the important v7 property:

    data stack and call linkage are isolated

while restoring bounded recursion:

    recursive CALL cannot grow host heap without limit.

The limit is per thread.
