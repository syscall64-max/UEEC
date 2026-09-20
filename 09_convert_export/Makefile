default: input.ueec convert_export loader_convert_export

gen_export:
	gcc -Wall -Wextra -O2 -o gen_export gen_export.c

input.ueec: gen_export
	./gen_export arch/answer_x86_64.bin 10 le > input.asm
	nasm -f bin input.asm -o input.ueec

convert_export: convert_export.c ueec.h ueec_export.h
	gcc -Wall -Wextra -O2 -o convert_export convert_export.c

loader_convert_export: loader_convert_export.c ueec.h
	gcc -Wall -Wextra -O2 -o loader_convert_export loader_convert_export.c

# yvela mimarTuleba, x86-64-ze: compact -> flat -> compact (round-trip)
run-all-directions: default
	./convert_export input.ueec flat_from_compact.ueec --design flat
	./convert_export flat_from_compact.ueec back_to_compact.ueec --design compact
	./convert_export back_to_compact.ueec flat_again.ueec --design flat
	@echo "--- input.ueec (COMPACT_GROUP, originali) ---"
	./loader_convert_export input.ueec
	@echo "--- flat_from_compact.ueec (SIMPLE_FLAT) ---"
	./loader_convert_export flat_from_compact.ueec
	@echo "--- back_to_compact.ueec (COMPACT_GROUP, round-trip) ---"
	./loader_convert_export back_to_compact.ueec
	@echo "--- round-trip bit-identuroba (input.ueec == back_to_compact.ueec) ---"
	cmp input.ueec back_to_compact.ueec && echo "ᲖᲣᲡᲢᲐᲓ ᲘᲓᲔᲜᲢᲣᲠᲘა (cmp: sxvaoba ver mოiZebna)"
	@echo "--- SIMPLE_FLAT round-trip (flat_from_compact.ueec == flat_again.ueec) ---"
	cmp flat_from_compact.ueec flat_again.ueec && echo "ᲖᲣᲡᲢᲐᲓ ᲘᲓᲔᲜᲢᲣᲠᲘა (cmp: sxvaoba ver mოiZebna)"

# ---------------- arch/answer_<arqiteqtura>.bin -- TiToeuli target-isTvis
# cross-gcc-iT iTargmneba arch/answer.c-dan (int answer(void){return 42;})
# da objcopy-iT gamoiRweba mxolod .text (ara mTeli .o, sxva symbol-ebis
# gareSe). Tu .bin uk've arsebobs (winaswar aRebuli, am zip-Si CarTuli),
# make xelaxla ar aSenebs -- magram Tu ar arsebobs (magaliTad, riscv32,
# romlisTvisac es sesia toolchain-s ver aSenebda), make TviTon aSenebs,
# Tu Sesabamisi cross-gcc/objcopy xelmisawvdomia.
ANSWER_CFLAGS := -O2 -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector

arch/answer_arm32.bin: arch/answer.c
	arm-linux-gnueabihf-gcc -marm $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_arm32.o
	arm-linux-gnueabihf-objcopy -O binary --only-section=.text arch/answer_arm32.o $@

arch/answer_arm64.bin: arch/answer.c
	aarch64-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_arm64.o
	aarch64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_arm64.o $@

arch/answer_loongarch64.bin: arch/answer.c
	loongarch64-linux-gnu-gcc-14 $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_loongarch64.o
	loongarch64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_loongarch64.o $@

arch/answer_powerpc32.bin: arch/answer.c
	powerpc-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_powerpc32.o
	powerpc-linux-gnu-objcopy -O binary --only-section=.text arch/answer_powerpc32.o $@

arch/answer_powerpc64.bin: arch/answer.c
	powerpc64-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_powerpc64.o
	powerpc64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_powerpc64.o $@

arch/answer_riscv32.bin: arch/answer.c
	/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_riscv32.o
	/opt/riscv32/bin/riscv32-unknown-linux-gnu-objcopy -O binary --only-section=.text arch/answer_riscv32.o $@

arch/answer_riscv64.bin: arch/answer.c
	riscv64-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_riscv64.o
	riscv64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_riscv64.o $@

arch/answer_sparc32.bin: arch/answer.c
	sparc64-linux-gnu-gcc -m32 $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_sparc32.o
	sparc64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_sparc32.o $@

arch/answer_sparc64.bin: arch/answer.c
	sparc64-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_sparc64.o
	sparc64-linux-gnu-objcopy -O binary --only-section=.text arch/answer_sparc64.o $@

arch/answer_x86.bin: arch/answer.c
	i686-linux-gnu-gcc $(ANSWER_CFLAGS) -c arch/answer.c -o arch/answer_x86.o
	i686-linux-gnu-objcopy -O binary --only-section=.text arch/answer_x86.o $@

# ---------------- QEMU cross-architecture ----------------
# arq. arg-ebi: (1) label, (2) cross-gcc, (3) qemu, (4) UEEC_MACHINE_*, (5) code.bin, (6) be|le
define ARCH_TEST
$(1): gen_export convert_export loader_convert_export $(5)
	./gen_export $(5) $(4) $(6) > input_$(1).asm
	nasm -f bin input_$(1).asm -o input_$(1).ueec
	./convert_export input_$(1).ueec flat_$(1).ueec --design flat
	$(2) -Wall -Wextra -O2 -static -o loader_$(1) loader_convert_export.c
	$(3) ./loader_$(1) input_$(1).ueec
	$(3) ./loader_$(1) flat_$(1).ueec
endef

$(eval $(call ARCH_TEST,arm32,arm-linux-gnueabihf-gcc,qemu-arm,0,arch/answer_arm32.bin,le))
$(eval $(call ARCH_TEST,arm64,aarch64-linux-gnu-gcc,qemu-aarch64,1,arch/answer_arm64.bin,le))
$(eval $(call ARCH_TEST,longarm64,loongarch64-linux-gnu-gcc-14,qemu-loongarch64,2,arch/answer_loongarch64.bin,le))
$(eval $(call ARCH_TEST,powerpc32,powerpc-linux-gnu-gcc,qemu-ppc,3,arch/answer_powerpc32.bin,be))
$(eval $(call ARCH_TEST,powerpc64,powerpc64-linux-gnu-gcc,qemu-ppc64,4,arch/answer_powerpc64.bin,be))
$(eval $(call ARCH_TEST,riscv32,/opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc,qemu-riscv32,5,arch/answer_riscv32.bin,le))
$(eval $(call ARCH_TEST,riscv64,riscv64-linux-gnu-gcc,qemu-riscv64,6,arch/answer_riscv64.bin,le))
$(eval $(call ARCH_TEST,sparc32,sparc64-linux-gnu-gcc -m32,qemu-sparc32plus,7,arch/answer_sparc32.bin,be))
$(eval $(call ARCH_TEST,sparc64,sparc64-linux-gnu-gcc,qemu-sparc64,8,arch/answer_sparc64.bin,be))
$(eval $(call ARCH_TEST,x86,i686-linux-gnu-gcc,qemu-i386,9,arch/answer_x86.bin,le))

x86_64: run-all-directions

all: arm32 arm64 riscv64 powerpc32 powerpc64 sparc32 sparc64 longarm64 x86 x86_64 riscv32

clean:
	rm -f input.asm input.ueec input_*.asm input_*.ueec *.ueec gen_export convert_export loader_convert_export \
	      arch/*.o \
	      loader_arm32 loader_arm64 loader_riscv64 loader_riscv32 \
	      loader_powerpc32 loader_powerpc64 loader_sparc32 loader_sparc64 loader_longarm64 loader_x86
