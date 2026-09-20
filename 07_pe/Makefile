gen_pe:
	gcc -Wall -Wextra -O2 -o gen_pe gen_pe.c

loader_pe:
	gcc -Wall -Wextra -O2 -o loader_pe loader_pe.c

# arch-indexebi: 0=arm32 1=arm64 2=longarm64 3=powerpc32 4=powerpc64
#                5=riscv32 6=riscv64 7=sparc32 8=sparc64 9=x86 10=x86_64

run-x86_64: gen_pe loader_pe
	./gen_pe 10 > node_x86_64.asm
	nasm -f bin node_x86_64.asm -o node_x86_64.ueec
	./loader_pe node_x86_64.ueec

# ---------------- QEMU cross-architecture ----------------
define ARCH_TEST
$(1): gen_pe loader_pe
	./gen_pe $(4) > node_$(1).asm
	nasm -f bin node_$(1).asm -o node_$(1).ueec
	$(2) -Wall -Wextra -O2 -static -o loader_pe_$(1) loader_pe.c
	$(3) ./loader_pe_$(1) node_$(1).ueec
endef

$(eval $(call ARCH_TEST,arm32,arm-linux-gnueabihf-gcc,qemu-arm,0))
$(eval $(call ARCH_TEST,arm64,aarch64-linux-gnu-gcc,qemu-aarch64,1))
$(eval $(call ARCH_TEST,longarm64,loongarch64-linux-gnu-gcc-14,qemu-loongarch64,2))
$(eval $(call ARCH_TEST,powerpc32,powerpc-linux-gnu-gcc,qemu-ppc,3))
$(eval $(call ARCH_TEST,powerpc64,powerpc64-linux-gnu-gcc,qemu-ppc64,4))
$(eval $(call ARCH_TEST,riscv32,/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc,qemu-riscv32,5))
$(eval $(call ARCH_TEST,riscv64,riscv64-linux-gnu-gcc,qemu-riscv64,6))
$(eval $(call ARCH_TEST,sparc32,sparc64-linux-gnu-gcc -m32,qemu-sparc32plus,7))
$(eval $(call ARCH_TEST,sparc64,sparc64-linux-gnu-gcc,qemu-sparc64,8))
$(eval $(call ARCH_TEST,x86,i686-linux-gnu-gcc,qemu-i386,9))

x86_64: run-x86_64

all: arm32 arm64 longarm64 powerpc32 powerpc64 sparc32 sparc64 x86 x86_64 riscv64 riscv32

clean:
	rm -f node_*.ueec node_*.asm gen_pe loader_pe \
	      loader_pe_arm32 loader_pe_arm64 loader_pe_longarm64 loader_pe_powerpc32 loader_pe_powerpc64 \
	      loader_pe_riscv32 loader_pe_riscv64 loader_pe_sparc32 loader_pe_sparc64 loader_pe_x86
