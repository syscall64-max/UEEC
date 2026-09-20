TESTS := \
	test_arm32 \
	test_arm64 \
	test_longarm64 \
	test_powerpc32 \
	test_powerpc64 \
	test_riscv32 \
	test_riscv64 \
	test_sparc32 \
	test_sparc64 \
	test_x86 \
	test_x86_64

.PHONY: all $(TESTS) clean install
.NOTPARALLEL:

all: $(TESTS)
	@echo "ყველა ტესტი წარმატებით დასრულდა"

test_arm32:
	nasm -f bin arm32.asm -o arm32.bin && arm-linux-gnueabihf-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-arm ./loader arm32.bin

test_arm64:
	nasm -f bin arm64.asm -o arm64.bin && aarch64-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-aarch64 ./loader arm64.bin

test_longarm64:
	nasm -f bin longarm64.asm -o longarm64.bin && loongarch64-linux-gnu-gcc-14 -Wall -Wextra -O2 -static loader.c -o loader && qemu-loongarch64 ./loader longarm64.bin

test_powerpc32:
	nasm -f bin powerpc32.asm -o powerpc32.bin && powerpc-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-ppc ./loader powerpc32.bin

test_powerpc64:
	nasm -f bin powerpc64.asm -o powerpc64.bin && powerpc64-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-ppc64 ./loader powerpc64.bin

test_riscv32:
	nasm -f bin riscv32.asm -o riscv32.bin && /opt/riscv32/bin/riscv32-unknown-linux-gnu-gcc -march=rv32gc -mabi=ilp32d -std=gnu11 -Wall -Wextra -O2 -static loader.c -o loader && qemu-riscv32 ./loader riscv32.bin

test_riscv64:
	nasm -f bin riscv64.asm -o riscv64.bin && riscv64-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-riscv64 ./loader riscv64.bin

test_sparc32:
	nasm -f bin sparc32.asm -o sparc32.bin && sparc64-linux-gnu-gcc -m32 -Wall -Wextra -O2 -static loader.c -o loader && qemu-sparc32plus ./loader sparc32.bin

test_sparc64:
	nasm -f bin sparc64.asm -o sparc64.bin && sparc64-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-sparc64 ./loader sparc64.bin

test_x86:
	nasm -f bin x86.asm -o x86.bin && i686-linux-gnu-gcc -Wall -Wextra -O2 -static loader.c -o loader && qemu-i386 ./loader x86.bin

test_x86_64:
	nasm -f bin x86-64.asm -o x86-64.bin && gcc loader.c -o loader && ./loader x86-64.bin

clean:
	rm -f *.bin loader

install:
	apt-get update
	apt-get install -y \
		nasm \
		qemu-user \
		gcc-i686-linux-gnu \
		gcc-multilib-sparc64-linux-gnu \
		gcc-arm-linux-gnueabihf \
		gcc-aarch64-linux-gnu \
		gcc-riscv64-linux-gnu \
		gcc-powerpc-linux-gnu \
		gcc-powerpc64-linux-gnu \
		gcc-14-loongarch64-linux-gnu
	@echo ""
	@echo "=== დაინსტალირდა ==="
	@echo "arm32     -> arm-linux-gnueabihf-gcc + qemu-arm"
	@echo "arm64     -> aarch64-linux-gnu-gcc + qemu-aarch64"
	@echo "riscv64   -> riscv64-linux-gnu-gcc + qemu-riscv64"
	@echo "powerpc32 -> powerpc-linux-gnu-gcc + qemu-ppc"
	@echo "powerpc64 -> powerpc64-linux-gnu-gcc + qemu-ppc64"
	@echo "longarm64 -> loongarch64-linux-gnu-gcc-14 + qemu-loongarch64"
	@echo "sparc32   -> sparc64-linux-gnu-gcc -m32 + qemu-sparc32plus"
	@echo "sparc64   -> sparc64-linux-gnu-gcc + qemu-sparc64"
	@echo "x86       -> i686-linux-gnu-gcc + qemu-i386"
	@echo "x86-64    -> gcc (native)"
	@echo ""
	@echo "=== ცალკე ხელით საჭირო ==="
	@echo "riscv32   -> /opt/riscv32 toolchain (apt-ში არაა, ცალკე უნდა დააყენო)"
	@echo ""
	@echo "=== პროექტიდან ამოღებულია ==="
	@echo "mips32/mips64 -> gcc-mips-linux-gnu / gcc-mips64-linux-gnuabi64"
	@echo "             Debian/Ubuntu-მ სრულად მოაცილა MIPS architecture-ის"
	@echo "             cross-toolchain პაკეტები repo-დან (mipsel 2023, mips64el/armel 2025)."
