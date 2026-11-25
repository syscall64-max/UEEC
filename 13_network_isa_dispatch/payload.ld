SECTIONS {
    . = 0;
    .text : { *(.text.ueec_sum) *(.text .text.*) }
    .forbidden : { *(.rodata .rodata.* .data .data.* .bss .bss.* .sdata* .sbss* .got*) }
    /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) *(.opd) *(.gnu.attributes) }
}
ASSERT(SIZEOF(.forbidden) == 0, "payload must not require a separate data/constant section")
