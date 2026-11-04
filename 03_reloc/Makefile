default:
	nasm -f bin 03_reloc.asm -o 03_reloc.ueec

loader:
	gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -O2 -o loader loader.c

run-mono: default loader
	./loader --L=monolithic 03_reloc.ueec

run-scattered: default loader
	./loader --L=scattered 03_reloc.ueec

# ---------------- QEMU cross-architecture ----------------
define ARCH_TEST
$(1): default
	$(2) -Wall -Wextra -O2 -static -o loader_$(1) loader.c
	$(3) ./loader_$(1) --L=monolithic 03_reloc.ueec
	$(3) ./loader_$(1) --L=scattered 03_reloc.ueec
endef

$(eval $(call ARCH_TEST,arm32,arm-linux-gnueabihf-gcc,qemu-arm))
$(eval $(call ARCH_TEST,arm64,aarch64-linux-gnu-gcc,qemu-aarch64))
$(eval $(call ARCH_TEST,riscv64,riscv64-linux-gnu-gcc,qemu-riscv64))
$(eval $(call ARCH_TEST,riscv32,/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc,qemu-riscv32))
$(eval $(call ARCH_TEST,powerpc32,powerpc-linux-gnu-gcc,qemu-ppc))
$(eval $(call ARCH_TEST,powerpc64,powerpc64-linux-gnu-gcc,qemu-ppc64))
$(eval $(call ARCH_TEST,sparc32,sparc64-linux-gnu-gcc -m32,qemu-sparc32plus))
$(eval $(call ARCH_TEST,sparc64,sparc64-linux-gnu-gcc,qemu-sparc64))
$(eval $(call ARCH_TEST,longarm64,loongarch64-linux-gnu-gcc-14,qemu-loongarch64))
$(eval $(call ARCH_TEST,x86,i686-linux-gnu-gcc,qemu-i386))
$(eval $(call ARCH_TEST,s390x,s390x-linux-gnu-gcc,qemu-s390x))

x86_64: run-mono run-scattered

all: arm32 arm64 riscv64 powerpc32 powerpc64 sparc32 sparc64 longarm64 x86 x86_64 riscv32 s390x

clean:
	rm -f 03_reloc.ueec loader loader_arm32 loader_arm64 loader_riscv64 loader_riscv32 \
	      loader_powerpc32 loader_powerpc64 loader_sparc32 loader_sparc64 loader_longarm64 loader_x86 loader_s390x gen

# Rebuild NASM source from the versioned architecture payloads.
regen: gen.c $(wildcard arch/*.bin)
	$(CC) -std=c11 -Wall -Wextra -O2 -o gen gen.c
	./gen > 03_reloc.asm

check-s390x: s390x
	@echo "03_reloc S390X monolithic/scattered PASS"

.PHONY: regen check-s390x

# Optional verification that regenerating the S390X bytes preserves the payload.
verify-s390x-bytes:
	s390x-linux-gnu-as -o arch/s390x.o arch/s390x.s
	s390x-linux-gnu-objcopy -O binary -j .text arch/s390x.o arch/s390x.generated.bin
	cmp arch/s390x.bin arch/s390x.generated.bin
	rm -f arch/s390x.o arch/s390x.generated.bin
	@echo "S390X raw CODE bytes match assembler source"

.PHONY: verify-s390x-bytes
