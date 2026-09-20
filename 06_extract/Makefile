default: multi.ueec extract loader

multi.ueec: 03_reloc.asm
	nasm -f bin 03_reloc.asm -o multi.ueec

extract: extract.c ueec.h
	gcc -Wall -Wextra -O2 -o extract extract.c

loader: loader.c ueec.h
	gcc -Wall -Wextra -O2 -o loader loader.c

run-x86_64: default
	./extract multi.ueec node_x86_64.ueec --machine 10
	./loader --L=monolithic node_x86_64.ueec
	./loader --L=scattered node_x86_64.ueec

run-combo: default
	./extract multi.ueec node_combo.ueec --machine-type 0 --machine 9
	i686-linux-gnu-gcc -Wall -Wextra -O2 -static -o loader_x86 loader.c
	qemu-i386 ./loader_x86 --L=monolithic node_combo.ueec

run-ambiguous: default
	-./extract multi.ueec node_bad.ueec --system-type 2 --system 2

# ---------------- QEMU cross-architecture ----------------
define ARCH_TEST
$(1): default
	./extract multi.ueec node_$(1).ueec --machine $(4)
	$(2) -Wall -Wextra -O2 -static -o loader_$(1) loader.c
	$(3) ./loader_$(1) --L=monolithic node_$(1).ueec
	$(3) ./loader_$(1) --L=scattered node_$(1).ueec
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
	rm -f multi.ueec node_*.ueec extract loader loader_x86 \
	      loader_arm32 loader_arm64 loader_longarm64 loader_powerpc32 loader_powerpc64 \
	      loader_riscv32 loader_riscv64 loader_sparc32 loader_sparc64
