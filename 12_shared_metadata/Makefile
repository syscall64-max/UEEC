PYTHON ?= python3
ARCH ?= $(shell $(PYTHON) -c 'from architectures import native_arch; print(native_arch())')

.PHONY: all loader loaders payloads payloads-native generate test test-all test-sanitize inspect clean
all: loader generate

loader:
	$(PYTHON) tools/build.py loader --arch $(ARCH)

loaders:
	$(PYTHON) tools/build.py loader --arch all

# Prebuilt leaf CODE is included with SHA-256 manifests. Rebuild explicitly.
payloads:
	$(PYTHON) tools/build.py payloads --arch all

payloads-native:
	$(PYTHON) tools/build.py payloads --arch $(ARCH)

generate:
	$(PYTHON) generate.py

inspect: all
	./build/ueec-loader --groups
	./build/ueec-loader --list-technologies build/sum.ueec

test: all
	$(PYTHON) test_dispatch.py --arch $(ARCH)

test-all: loaders generate
	$(PYTHON) tools/test_all.py

# Host-only parser/dispatch regression with memory/undefined-behavior checks.
test-sanitize: generate
	mkdir -p build
	$(CC) -std=c11 -O1 -g -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer loader.c -o build/ueec-loader-sanitize
	$(PYTHON) test_dispatch.py --arch $(ARCH) --loader build/ueec-loader-sanitize

clean:
	rm -rf build
