/* IBM Z / s390x: int answer(void) { return 42; } */
.text
.globl answer
.type answer,@function
answer:
    lghi %r2,42
    br %r14
.size answer,.-answer
